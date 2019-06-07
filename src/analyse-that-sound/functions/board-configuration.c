/* \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\ Includes */
#include "analyse-that-sound/functions/board-configuration.h"

#include "analyse-that-sound/constants.h"
#include "analyse-that-sound/global-variables.h"
#include "analyse-that-sound/macros.h"
#include "analyse-that-sound/typedefs.h"

#include <arm_math.h>
#include <arm_const_structs.h>

#include <LPC17xx.h>

#include <lpc17xx_adc.h>
#include <lpc17xx_clkpwr.h>
#include <lpc17xx_dac.h>
#include <lpc17xx_i2c.h>
#include <lpc17xx_pinsel.h>
#include <lpc17xx_ssp.h>

#include <oled.h>


/* \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\ Board configuration */
void configureBoard(void)
{
    configureSystemClock();
    configurePeripherials();
    configureInterruptsInNVIC();
}

static
void configureSystemClock(void)
{
    /* Enable main oscillator */
    LPC_SC->SCS = BIT(5);                             /* [@user-manual:3.7.1] */

    /* Wait for main oscillator to be ready */
    while ((LPC_SC->SCS & BIT(6)) == 0) {}            /* [@user-manual:3.7.1] */

    /* Set clock divisor: 3 */
    LPC_SC->CCLKCFG = 2;                              /* [@user-manual:4.7.1] */

    /* Set default peripheral clock divider: 4 */
    LPC_SC->PCLKSEL0 = 0;                             /* [@user-manual:4.7.3] */
    LPC_SC->PCLKSEL1 = 0;                             /* [@user-manual:4.7.3] */

    /* Select main oscillator as clock source for PLL0 */
    LPC_SC->CLKSRCSEL = 1;                            /* [@user-manual:4.4.1] */

    /* Configure PLL0
        > Target clock frequency:       100 MHz
          PLL0 input frequency, (Fin):   12 MHz
          PLL0 output frequency (Fout): 300 MHz (must be in range
                                                 from 275 to 550 MHz
                                                 so we are using further clock
                                                 division (CLKCFG = 2))
        > M = (Fcco * N) / (2 * Fin)
          chosen possible integer solution:
          M: 25
          N: 2
        > A correct feed sequence must be written to the
          PLL0FEED register in order for changes to the
          PLL0CON and PLL0CFG registers to take effect */
    uint16_t M = 25u;
    uint16_t N = 2u;

    LPC_SC->PLL0CFG = ((N - (uint16_t)1)              /* [@user-manual:4.5.4] */
                       << (uint16_t)16)
                      | (M - (uint16_t)1);

    LPC_SC->PLL0FEED = 0xAA;                          /* [@user-manual:4.5.8] */
    LPC_SC->PLL0FEED = 0x55;                          /* [@user-manual:4.5.8] */

    /* Enable PLL0 */
    LPC_SC->PLL0CON = BIT(0);                         /* [@user-manual:4.5.3] */

    LPC_SC->PLL0FEED = 0xAA;                          /* [@user-manual:4.5.8] */
    LPC_SC->PLL0FEED = 0x55;                          /* [@user-manual:4.5.8] */

    /* Wait for PLOCK0 */
    while (!(LPC_SC->PLL0STAT & BIT(26))) {}          /* [@user-manual:4.5.5] */

    /* Enable and connect PLL0 */
    LPC_SC->PLL0CON = BIT(1) | BIT(0);                /* [@user-manual:4.5.3] */

    LPC_SC->PLL0FEED  = 0xAA;                         /* [@user-manual:4.5.8] */
    LPC_SC->PLL0FEED  = 0x55;                         /* [@user-manual:4.5.8] */

    /* Wait for PLLC0_STAT and PLLE0_STAT */
    while (!(LPC_SC->PLL0STAT                         /* [@user-manual:4.5.5] */
             & ((BIT(25)) | (BIT(24))))) {}
}

static
void configurePeripherials(void)
{
    configureAndStartOLED();
    configureAndStartTimer0();
    configureAndStartTimer1();
    configureAndStartADC();
    configureAndStartDAC();
    configureAndStartSpeakerAmplifier();
    configureJoystick();
}

static
void configureAndStartOLED(void)
{
    init_ssp();
    init_i2c();
    oled_init();
}

static
void init_ssp(void)
{
    SSP_CFG_Type SSP_ConfigStruct;
    PINSEL_CFG_Type PinCfg;

    PinCfg.Funcnum = 2;
    PinCfg.OpenDrain = 0;
    PinCfg.Pinmode = 0;
    PinCfg.Portnum = 0;
    PinCfg.Pinnum = 7;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 8;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 9;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Funcnum = 0;
    PinCfg.Portnum = 2;
    PinCfg.Pinnum = 2;
    PINSEL_ConfigPin(&PinCfg);

    SSP_ConfigStructInit(&SSP_ConfigStruct);

    // Initialize SSP peripheral with parameter given in structure above
    SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

    // Enable SSP peripheral
    SSP_Cmd(LPC_SSP1, ENABLE);

}

static
void init_i2c(void)
{
    PINSEL_CFG_Type PinCfg;

    // Initialize I2C2 pin connect
    PinCfg.Funcnum = 2;
    PinCfg.Pinnum = 10;
    PinCfg.Portnum = 0;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 11;
    PINSEL_ConfigPin(&PinCfg);

    // Initialize I2C peripheral
    I2C_Init(LPC_I2C2, 100000);

    I2C_Cmd(LPC_I2C2, ENABLE);
}

static
void configureAndStartTimer0(void)
{
    /* Turn on power */
    CLKPWR_ConfigPPWR(CLKPWR_PCONP_PCTIM0, ENABLE);

    /* Select clock divisor */
    CLKPWR_SetPCLKDiv(CLKPWR_PCLKSEL_TIMER0, CLKPWR_PCLKSEL_CCLK_DIV_4);

    /* Reset counters */
    LPC_TIM0->TCR |= BIT(1);
    LPC_TIM0->TCR &= ~BIT(1);

    /* Set prescaler */
    LPC_TIM0->PR = 0;

    /* Configure external match for MAT0.1 */
    LPC_TIM0->EMR |= BIT(7);
    LPC_TIM0->EMR |= BIT(6);

    /* Configure interrupts */
    LPC_TIM0->MR1 = 25000000 / SAMPLE_RATE / 2;		// When counter reaches match register value (8000 Hz)
    LPC_TIM0->MCR |= BIT(4);						// then reset
    /* LPC_TIM0->MCR |= BIT(3); */					// then interrupt

    /* Start */
    LPC_TIM0->TCR |= BIT(0);
}

static
void configureAndStartTimer1(void)
{
    /* Turn on power */
    CLKPWR_ConfigPPWR(CLKPWR_PCONP_PCTIM1,
                      ENABLE);

    /* Select clock divisor */
    CLKPWR_SetPCLKDiv(CLKPWR_PCLKSEL_TIMER1,
                      CLKPWR_PCLKSEL_CCLK_DIV_4);

    /* Reset counters */
    LPC_TIM1->TCR |= BIT(1);
    LPC_TIM1->TCR &= ~BIT(1);

    /* Set prescaler */
    LPC_TIM1->PR = 0;

    /* Configure interrupts */
    LPC_TIM1->MR1 = 25000000 / 100000;			// When counter reaches match register value (100 000 Hz)
    LPC_TIM1->MCR |= BIT(4);				// then reset
    LPC_TIM1->MCR |= BIT(3);				// then interrupt

    /* Start */
    LPC_TIM1->TCR |= BIT(0);
}

static
void configureAndStartADC(void)
{
    /* Turn on power */
    CLKPWR_ConfigPPWR(CLKPWR_PCONP_PCAD,
                      ENABLE);

    /* Select clock divisor */
    CLKPWR_SetPCLKDiv(CLKPWR_PCLKSEL_ADC,
                      CLKPWR_PCLKSEL_CCLK_DIV_4);

    /* Confiugre pin */
    LPC_PINCON->PINSEL1 &= ~(BIT(14) | BIT(15));
    LPC_PINCON->PINSEL1 |= BIT(14);

    /* Reset ADCR */
    LPC_ADC->ADCR = 0;
    LPC_ADC->ADCR |= BIT(21);

    /* Select channel */
    LPC_ADC->ADCR |= BIT(0);

    /* Select additional clock divisor */
    LPC_ADC->ADCR |= BIT(8); 		// Other bits are already 0 since reset, clock divisor: 2

    /* Enable interrupts on channel 0 */
    LPC_ADC->ADINTEN |= BIT(0);

    /* Disable burst mode */
    LPC_ADC->ADCR &= ~BIT(16);

    /* Start conversion when edge occurs on MAT0.1 */
    LPC_ADC->ADCR |= BIT(26);
    LPC_ADC->ADCR &= ~BIT(25);
    LPC_ADC->ADCR &= ~BIT(24);
}

static
void configureAndStartDAC(void)
{
    /* Turn on power and configureBoard pin */
    LPC_PINCON->PINSEL1 &= ~(BIT(21) | BIT(20));
    LPC_PINCON->PINSEL1 |= BIT(21);

    /* Select clock divisor */
    CLKPWR_SetPCLKDiv(CLKPWR_PCLKSEL_DAC,
                      CLKPWR_PCLKSEL_CCLK_DIV_4);
}

static
void configureAndStartSpeakerAmplifier(void)
{
    GPIO_SetDir(0, BIT(27), 1);
    GPIO_SetDir(0, BIT(28), 1);
    GPIO_SetDir(2, BIT(13), 1);

    GPIO_ClearValue(0, BIT(27));	//LM4811-clk
    GPIO_ClearValue(0, BIT(28));	//LM4811-up/dn
    GPIO_ClearValue(2, BIT(13));	//LM4811-shutdn
}

static
void configureJoystick(void)
{
    //buttons address
    //GPIO 0_15, 0_16, 0_17, 2_3, 2_4

    //set direction - input
    LPC_GPIO0->FIODIR &= ~BIT(15);	//top
    LPC_GPIO0->FIODIR &= ~BIT(16);	//left
    LPC_GPIO0->FIODIR &= ~BIT(17);	//center
    LPC_GPIO2->FIODIR &= ~BIT(3);	//bottom
    LPC_GPIO2->FIODIR &= ~BIT(4);	//right
}

static
void configureInterruptsInNVIC(void)
{
    /* Disable TIMER0 interrupts */
    NVIC->ICER[0] |= BIT(1);                          /* [@user-manual:6.5.3] */

    /* Enable TIMER1 interrupts */
    NVIC->ISER[0] |= BIT(2);                          /* [@user-manual:6.5.1] */

    /* Enable ADC interrupts */
    NVIC->ISER[0] |= BIT(22);                         /* [@user-manual:6.5.1] */
}


/* \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\ */