/**
 ******************************************************************************
 * @file    Audio_playback_and_record/src/main.c
 * @author  MCD Application Team
 * @version V1.0.0
 * @date    28-October-2011
 * @brief   Main program body
 ******************************************************************************
 * @attention
 *
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/** @addtogroup STM32F4-Discovery_Audio_Player_Recorder
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* SPI Configuration defines */
#define SPI_SCK_PIN                       GPIO_Pin_10
#define SPI_SCK_GPIO_PORT                 GPIOB
#define SPI_SCK_GPIO_CLK                  RCC_AHB1Periph_GPIOB
#define SPI_SCK_SOURCE                    GPIO_PinSource10
#define SPI_SCK_AF                        GPIO_AF_SPI2

#define SPI_MOSI_PIN                      GPIO_Pin_3
#define SPI_MOSI_GPIO_PORT                GPIOC
#define SPI_MOSI_GPIO_CLK                 RCC_AHB1Periph_GPIOC
#define SPI_MOSI_SOURCE                   GPIO_PinSource3
#define SPI_MOSI_AF                       GPIO_AF_SPI2

#define INTERNAL_BUFF_SIZE      64
#define PCM_DECIMATION_SIZE	16
#define PCM_OUT_SIZE            PCM_DECIMATION_SIZE * 20

/* Private variables ---------------------------------------------------------*/
RCC_ClocksTypeDef RCC_Clocks;

uint16_t CCR_Val = 1;

static uint16_t InternalBuffer[INTERNAL_BUFF_SIZE];

static uint32_t InternalBufferSize = 0;

PDMFilter_InitStruct Filter;

uint16_t RecBuf[PCM_OUT_SIZE];

uint16_t PCMBufOffset = 0;

uint8_t CaptureComplete = 0;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
static void WaveCapture_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Enable GPIO clocks */
	RCC_AHB1PeriphClockCmd(SPI_SCK_GPIO_CLK | SPI_MOSI_GPIO_CLK, ENABLE);

	/* Enable GPIO clocks */
	RCC_AHB1PeriphClockCmd(SPI_SCK_GPIO_CLK | SPI_MOSI_GPIO_CLK, ENABLE);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	/* SPI SCK pin configuration */
	GPIO_InitStructure.GPIO_Pin = SPI_SCK_PIN;
	GPIO_Init(SPI_SCK_GPIO_PORT, &GPIO_InitStructure);

	/* Connect SPI pins to AF5 */
	GPIO_PinAFConfig(SPI_SCK_GPIO_PORT, SPI_SCK_SOURCE, SPI_SCK_AF );

	/* SPI MOSI pin configuration */
	GPIO_InitStructure.GPIO_Pin = SPI_MOSI_PIN;
	GPIO_Init(SPI_MOSI_GPIO_PORT, &GPIO_InitStructure);
	GPIO_PinAFConfig(SPI_MOSI_GPIO_PORT, SPI_MOSI_SOURCE, SPI_MOSI_AF );
}

static void WaveCapture_NVIC_Init(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_3 );
	/* Configure the SPI interrupt priority */
	NVIC_InitStructure.NVIC_IRQChannel = SPI2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

static void WaveCapture_SPI_Init()
{
	I2S_InitTypeDef I2S_InitStructure;

	/* Enable the SPI clock */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

	/* SPI configuration */
	SPI_I2S_DeInit(SPI2 );
	I2S_InitStructure.I2S_AudioFreq = 32000;
	I2S_InitStructure.I2S_Standard = I2S_Standard_LSB;
	I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_16b;
	I2S_InitStructure.I2S_CPOL = I2S_CPOL_High;
	I2S_InitStructure.I2S_Mode = I2S_Mode_MasterRx;
	I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
	/* Initialize the I2S peripheral with the structure above */
	I2S_Init(SPI2, &I2S_InitStructure);

	/* Enable the Rx buffer not empty interrupt */
	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, ENABLE);
}

void WaveCaptureInit(void)
{
	/* Enable CRC module */
	RCC ->AHB1ENR |= RCC_AHB1ENR_CRCEN;

	/* Filter LP & HP Init */
	Filter.LP_HZ = 8000;
	Filter.HP_HZ = 10;
	Filter.Fs = 16000;
	Filter.Out_MicChannels = 1;
	Filter.In_MicChannels = 1;

	PDM_Filter_Init((PDMFilter_InitStruct *) &Filter);

	WaveCapture_GPIO_Init();
	WaveCapture_NVIC_Init();
	WaveCapture_SPI_Init();

	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, ENABLE);
}

void WaveCaptureStart(void)
{
	InternalBufferSize = 0;
	PCMBufOffset = 0; 
	I2S_Cmd(SPI2, ENABLE);
}

void WaveCaptureStop(void)
{
	I2S_Cmd(SPI2, DISABLE);
}

int main(void)
{
	/* Initialize LEDS */
	STM_EVAL_LEDInit(LED3);
	STM_EVAL_LEDInit(LED4);
	STM_EVAL_LEDInit(LED5);
	STM_EVAL_LEDInit(LED6);

	/* Green Led On: start of application */
	STM_EVAL_LEDOn(LED4);

	/* SysTick end of count event each 10ms */
	RCC_GetClocksFreq(&RCC_Clocks);
	SysTick_Config(RCC_Clocks.HCLK_Frequency / 100);

	/* Initialize User Button */
	STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_EXTI);

	WaveCaptureInit();
	WaveCaptureStart();

	while (1)
	{
		if (CaptureComplete == 1)
		{
			CaptureComplete = 0;
			WaveCaptureStop();
			//Analyse the knock
			WaveCaptureStart();
		}
	}

	return 0;
}

void SPI2_IRQHandler(void)
{
	u16 volume;
	u16 app;

	/* Check if data are available in SPI Data register */
	if (SPI_GetITStatus(SPI2, SPI_I2S_IT_RXNE ) != RESET)
	{
		app = SPI_I2S_ReceiveData(SPI2 );
		InternalBuffer[InternalBufferSize++] = HTONS(app);

		/* Check to prevent overflow condition */
		if (InternalBufferSize >= INTERNAL_BUFF_SIZE)
		{
			InternalBufferSize = 0;

			volume = 32;

			PDM_Filter_64_LSB((uint8_t *) InternalBuffer, RecBuf + PCMBufOffset, volume, (PDMFilter_InitStruct *) &Filter);
			PCMBufOffset += PCM_DECIMATION_SIZE;
			if (PCMBufOffset == PCM_OUT_SIZE)
			{
				PCMBufOffset = 0;
				CaptureComplete = 1;
			}
		}
	}
}

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
