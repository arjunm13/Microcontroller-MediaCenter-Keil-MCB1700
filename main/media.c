#include "LPC17xx.h"                        /* LPC17xx definitions */
#include "type.h"

#include "usb.h"
#include "usbcfg.h"
#include "usbhw.h"
#include "usbcore.h"
#include "usbaudio.h"

#include <stdio.h>  
#include <stdlib.h>
#include "GLCD.h"
#include "LED.h"
#include "ADC.h"
#include "KBD.h"

#define wait 5000000

#define blocksize 16
#define xBoardLeft 80
#define xBoardRight 240

#define screenX 320
#define screenY 240

#define LINE 0
#define SQUARE 1
#define ONE 2
#define THREE 3



extern  void SystemClockUpdate(void);
extern uint32_t SystemFrequency;  
extern unsigned char computers [];
//extern unsigned char jays [];
//extern unsigned char toronto [];
extern unsigned char Gallery [];
extern unsigned char GalleryBlur [];
extern unsigned char audioImage [];
extern unsigned char audioImageBlur [];
extern unsigned char tetris [];
extern unsigned char tetrisBlur [];
extern unsigned char mainBack [];
extern unsigned char background [];
extern unsigned char yellow2 [];
extern unsigned char red2 [];
extern unsigned char player [];
char text[10];		

void startMenu(void);
void game(void);

uint8_t  Mute;                                 /* Mute State */
uint32_t Volume;                               /* Volume Level */

uint8_t board[10][15];

#define __FI        1                      /* Font index 16x24               */
#define __USE_LCD   0										/* Uncomment to use the LCD */

#if USB_DMA
uint32_t *InfoBuf = (uint32_t *)(DMA_BUF_ADR);
short *DataBuf = (short *)(DMA_BUF_ADR + 4*P_C);
#else
uint32_t InfoBuf[P_C];
short DataBuf[B_S];                         /* Data Buffer */
#endif

uint16_t  DataOut;                              /* Data Out Index */
uint16_t  DataIn;                               /* Data In Index */

uint8_t   DataRun;                              /* Data Stream Run State */
uint16_t  PotVal;                               /* Potenciometer Value */
uint32_t  VUM;                                  /* VU Meter */
uint32_t  Tick;                                 /* Time Tick */

int selector=0;
uint32_t currentDir = 0;
int score;

struct tetris {
	char **game;
	int w;
	int h;
	int level;
	int gameover;
	int score;
	struct tetris_block {
		int type;
		int data[4][4];
		int w;
		int h;
	} currentShape;
};

struct tetris_block blocks[] =
	{	
		{LINE,{{1,1,1,1}},
		  4, 1},
		{SQUARE,{{1,1}, {1,1}},
		  2, 2
		},
		{ONE,{{1},{1}},
		  1, 3},
		{THREE,{{1,1,1}},
		  3, 1}
	};


void get_potval (void) {
  uint32_t val;

  LPC_ADC->CR |= 0x01000000;              /* Start A/D Conversion */
  do {
    val = LPC_ADC->GDR;                   /* Read A/D Data Register */
  } while ((val & 0x80000000) == 0);      /* Wait for end of A/D Conversion */
  LPC_ADC->CR &= ~0x01000000;             /* Stop A/D Conversion */
  PotVal = ((val >> 8) & 0xF8) +          /* Extract Potenciometer Value */
           ((val >> 7) & 0x08);
}
void gamedelay(){ 
	long k, count = 0;
	for(k = 0; k < wait; k++){
					count++;
			}
	}
void menudelay(){ 
	long k, count = 0;
	for(k = 0; k < wait/2; k++){
					count++;
			}
	}

/*
 * Timer Counter 0 Interrupt Service Routine
 *   executed each 31.25us (32kHz frequency)
 */

void TIMER0_IRQHandler(void) 
{
  long  val;
  uint32_t cnt;

  if (DataRun) {                            /* Data Stream is running */
    val = DataBuf[DataOut];                 /* Get Audio Sample */
    cnt = (DataIn - DataOut) & (B_S - 1);   /* Buffer Data Count */
    if (cnt == (B_S - P_C*P_S)) {           /* Too much Data in Buffer */
      DataOut++;                            /* Skip one Sample */
    }
    if (cnt > (P_C*P_S)) {                  /* Still enough Data in Buffer */
      DataOut++;                            /* Update Data Out Index */
    }
    DataOut &= B_S - 1;                     /* Adjust Buffer Out Index */
    if (val < 0) VUM -= val;                /* Accumulate Neg Value */
    else         VUM += val;                /* Accumulate Pos Value */
    val  *= Volume;                         /* Apply Volume Level */
    val >>= 16;                             /* Adjust Value */
    val  += 0x8000;                         /* Add Bias */
    val  &= 0xFFFF;                         /* Mask Value */
  } else {
    val = 0x8000;                           /* DAC Middle Point */
  }

  if (Mute) {
    val = 0x8000;                           /* DAC Middle Point */
  }

  LPC_DAC->CR = val & 0xFFC0;             /* Set Speaker Output */

  if ((Tick++ & 0x03FF) == 0) {             /* On every 1024th Tick */
    get_potval();		/* Get Potenciometer Value */
		if(get_button()==0x08){
			NVIC_DisableIRQ(TIMER0_IRQn);
			NVIC_DisableIRQ(USB_IRQn);
			USB_Connect(FALSE);
			GLCD_Clear(Black);
			startMenu();
			
			
		}
    if (VolCur == 0x8000) {                 /* Check for Minimum Level */
      Volume = 0;                           /* No Sound */
    } else {
      Volume = VolCur * PotVal;             /* Chained Volume Level */
    }
    val = VUM >> 20;                        /* Scale Accumulated Value */
    VUM = 0;                                /* Clear VUM */
    if (val > 7) val = 7;                   /* Limit Value */
  }

  LPC_TIM0->IR = 1;                         /* Clear Interrupt Flag */
}
void startScreen(void){
	GLCD_Init();
	GLCD_Clear(Black);
	GLCD_Bitmap (  0,   0, 320, 240, mainBack);
	GLCD_Bitmap (  8,   84, 96, 72, audioImage);
	GLCD_Bitmap (  112,   84, 96, 72, tetris);
	GLCD_Bitmap (  216,   84, 96, 72, Gallery);
}
void reset(void){
				GLCD_DisplayString(6, 0, __FI, "      EXITING........");
				selector=0;
				startScreen();
}
void gameover(void){
				GLCD_DisplayString(5, 0, __FI, "      GAME OVER     ");
				selector=0;
				startScreen();
}

void startAudio(void){
  volatile uint32_t pclkdiv, pclk;
		GLCD_Bitmap (  0,   140, 320, 100, player);
  /* SystemClockUpdate() updates the SystemFrequency variable */
  SystemClockUpdate();

  LPC_PINCON->PINSEL1 &=~((0x03<<18)|(0x03<<20));  
  /* P0.25, A0.0, function 01, P0.26 AOUT, function 10 */
  LPC_PINCON->PINSEL1 |= ((0x01<<18)|(0x02<<20));

  /* Enable CLOCK into ADC controller */
  LPC_SC->PCONP |= (1 << 12);

  LPC_ADC->CR = 0x00200E04;		/* ADC: 10-bit AIN2 @ 4MHz */
  LPC_DAC->CR = 0x00008000;		/* DAC Output set to Middle Point */

  /* By default, the PCLKSELx value is zero, thus, the PCLK for
  all the peripherals is 1/4 of the SystemFrequency. */
  /* Bit 2~3 is for TIMER0 */
  pclkdiv = (LPC_SC->PCLKSEL0 >> 2) & 0x03;
  switch ( pclkdiv )
  {
	case 0x00:
	default:
	  pclk = SystemFrequency/4;
	break;
	case 0x01:
	  pclk = SystemFrequency;
	break; 
	case 0x02:
	  pclk = SystemFrequency/2;
	break; 
	case 0x03:
	  pclk = SystemFrequency/8;
	break;
  }

  LPC_TIM0->MR0 = pclk/DATA_FREQ - 1;	/* TC0 Match Value 0 */
  LPC_TIM0->MCR = 3;					/* TCO Interrupt and Reset on MR0 */
  LPC_TIM0->TCR = 1;					/* TC0 Enable */
  NVIC_EnableIRQ(TIMER0_IRQn);

  USB_Init();				/* USB Initialization */
  USB_Connect(TRUE);		/* USB Connect */

  /********* The main Function is an endless loop ***********/ 
  while( 1 ); 
}


void gallery(void){
int exit =0;
int galState =0;
uint32_t dirGallery = 0;
while(exit ==0){
	
if(galState==0){
	GLCD_Bitmap (  0,   0, 320,  240, computers);
	GLCD_DisplayString(9, 0, __FI, "     COMPUTERS     ");
} else 
if(galState==1){
	GLCD_Bitmap (  0,   70, 320,  100, player);
	GLCD_DisplayString(9, 0, __FI, "  MUSIC PLAYER IMG  ");
} else
if(galState==2){
	GLCD_Bitmap (  0,   0, 320,  240, mainBack);
	GLCD_DisplayString(9, 0, __FI, "   Main Background  ");
} else {
	galState=0;
}
	
	dirGallery=get_button();
	if (dirGallery==(KBD_LEFT&KBD_MASK)){
			reset();
			break;
		}
	if (dirGallery==(KBD_UP&KBD_MASK)){
			galState++;
			GLCD_Clear(Black);
	
			}
	}
}
void game_init(){
	
	int backx=80, backy=0;
	int boardx=0, boardy=0;
	
	
	while(backx<=xBoardRight-blocksize){
			while(backy<=screenY-blocksize){
				GLCD_Bitmap (  backx,   backy, blocksize, blocksize, background);
				backy = backy+blocksize;
			}
			backx = backx+blocksize;
			backy=0;
	}
	
	while(boardx<10){
			while(boardy<=15){
				board[boardx][boardy] =0;
				boardy = boardy +1;
			}
			boardx = boardx+1;
			boardy=0;
	}
}
int x_block_pos(int x_pos){
	return x_pos-80;
}	
void drawPeice(int shape, int x_pos, int y_pos, int x_prev ){
	int i=0,j=0;
	
	for(i = x_prev; i<(x_prev+(blocksize*blocks[shape].w)); i = i+blocksize){
			GLCD_Bitmap ( i, y_pos-(blocksize*blocks[shape].h), blocksize,  blocksize, background);
			GLCD_Bitmap ( i, (y_pos-blocksize), blocksize,  blocksize, background);
		}
	
	for(i = x_pos; i<(x_pos+(blocksize*blocks[shape].w)); i = i+blocksize){
		for(j = y_pos; j>(y_pos-(blocksize*blocks[shape].h)); j = j-blocksize){
			GLCD_Bitmap ( i, j, blocksize,  blocksize, yellow2);
		}
	}
}

int collision(int shape, int x_pos, int y_pos){
	int landedCounter, ylandedCounter;
	int collision=0; 
	for (landedCounter=0;landedCounter<blocks[shape].w;landedCounter++){
		if ((y_pos/16)<15 && board[(x_pos/16)+landedCounter][(y_pos/16)+1]!=1){
			collision=0;
		}	else {
				for (landedCounter=0;landedCounter<blocks[shape].w;landedCounter++){
					for(ylandedCounter=0;ylandedCounter<blocks[shape].h;ylandedCounter++){
						board[(x_pos/16)+landedCounter][(y_pos/16)-ylandedCounter] = 1;
					}
				}
				collision=1;
				}
			}
	return collision;
}
void dropAll(int yline){

//		
//		sprintf(text, "0x%02X", board[0][14]);
//		GLCD_DisplayString(0,  0, __FI,  (unsigned char *)text);		sprintf(text, "0x%02X", board[1][14]);
//		GLCD_DisplayString(1,  0, __FI,  (unsigned char *)text);		sprintf(text, "0x%02X", board[0][15]);//1
//		GLCD_DisplayString(2,  0, __FI,  (unsigned char *)text);		sprintf(text, "0x%02X", board[1][15]);
//		GLCD_DisplayString(3,  0, __FI,  (unsigned char *)text);		sprintf(text, "0x%02X", board[5][14]);//1
//		GLCD_DisplayString(4,  0, __FI,  (unsigned char *)text);
	}
void dropBricks(int clear, int j){
	LED_Out(255);
	menudelay();
	while(j>2){
		GLCD_Bitmap ((80+clear*blocksize), (j-1)*blocksize, blocksize,  blocksize, background);
		if(board[clear][j-1]!=0){
				board[clear][j] =1;
				GLCD_Bitmap ((80+clear*blocksize), (j-1)*blocksize, blocksize,  blocksize, yellow2);
		}else{
				board[clear][j] =0;
			}
		j--;
		}
}
void clearlines(void){
	int i, j, sum=0,clear; 
	for(j = 15; j>=0; j--){
		for(i = 0; i<=10; i++){
			sum = sum + board[i][j];
		}
		if(sum>9){
			score+=10;
			for(clear =0;clear<10;clear++){
				board[clear][j]=0;
				//GLCD_Bitmap ((80+clear*blocksize), (j-2)*blocksize, blocksize,  blocksize, background);
				//GLCD_Bitmap ((80+clear*blocksize), (j-1)*blocksize, blocksize,  blocksize, background);
				dropBricks(clear,j);
				LED_Out(0);
			}
			//dropAll(j);
		}
		
		sprintf(text, "%d", score);
		GLCD_DisplayString(8,  0, __FI,"SCORE");
		GLCD_DisplayString(9,  0, __FI,  (unsigned char *)text);
		sum =0;
	}
}
int dropPeice(struct tetris_block shape){
	int x_pos=160, y_pos, i, done=0;
	int x_prev=160;
	uint32_t gameButton = 0;
	y_pos = shape.h*blocksize;

	while(done==0){
		for(i=0;i<10;i++){
			if(board[i][1]==1){
						score=0;
						return 1;
						gameover();
						break;
			}
		}
		if (collision(shape.type,x_block_pos(x_pos), y_pos)!=1){
			drawPeice(shape.type, x_pos, y_pos,x_prev );
			clearlines();
			y_pos=y_pos+blocksize;
			x_prev=x_pos;
			
			gamedelay();
		} else {
			done=1;
		}
	gameButton=get_button();
	if (gameButton==(KBD_RIGHT&KBD_MASK)){
		if (x_pos<(xBoardRight-(blocksize*blocks[shape.type].w))){
			x_prev=x_pos;
			x_pos+=blocksize;
		}
	}
	if (gameButton==(KBD_LEFT&KBD_MASK)){
		if (x_pos>xBoardLeft){
			x_prev=x_pos;
			x_pos-=blocksize;
		}
	}
	if (gameButton==(KBD_SELECT)){
		score=0;
		return 1;
		reset();
		break;
	}
	}
}

void game_run(void){
	struct tetris session;
	session.gameover = 0;
	while(session.gameover == 0){
		session.currentShape=blocks[0];
		session.gameover = dropPeice(session.currentShape);
		session.currentShape=blocks[1];
		session.gameover = dropPeice(session.currentShape);
		session.currentShape=blocks[0];
		session.gameover = dropPeice(session.currentShape);
		session.currentShape=blocks[1];
		session.gameover = dropPeice(session.currentShape);
		session.currentShape=blocks[0];
		session.gameover = dropPeice(session.currentShape);
}
	

}
void game(void){
	int fill =0;
	GLCD_Bitmap (  0,   0, 320,  240, computers);
	game_init();
	game_run();
	reset();
	
}
int main (void)
{
KBD_Init();
LED_Init();
startScreen();
startMenu();
}

void startMenu(void){
while (1) {		/* Initialize graphical LCD (if enabled */
menudelay();
currentDir=get_button();

switch (selector){
	case 0:
			//GLCD_Bitmap (  0,   0, 320, 240, background);
			GLCD_Bitmap (  8,   84, 96, 72, audioImage);
			GLCD_Bitmap (  112,   84, 96, 72, tetrisBlur);
			GLCD_Bitmap (  216,   84, 96, 72, GalleryBlur);
		break;
	case 1:
			GLCD_Bitmap (  8,   84, 96, 72, audioImageBlur);
			GLCD_Bitmap (  112,   84, 96, 72, tetris);
			GLCD_Bitmap (  216,   84, 96, 72, GalleryBlur);
		break;
	case 2:
			GLCD_Bitmap (  8,   84, 96, 72, audioImageBlur);
			GLCD_Bitmap (  112,   84, 96, 72, tetrisBlur);
			GLCD_Bitmap (  216,   84, 96, 72, Gallery);
		
//		GLCD_SetBackColor(White);
//		GLCD_SetTextColor(Yellow);
//		GLCD_DisplayString(6, 0, __FI, "        Games       ");
//		GLCD_SetBackColor(Blue);
//		GLCD_SetTextColor(White);
//		GLCD_DisplayString(4, 0, __FI, "      MP3 player    ");
//		GLCD_DisplayString(5, 0, __FI, "    Photo Gallery   ");
		break;
	default:
		selector=0;
		break;
}

if (currentDir==(KBD_RIGHT&KBD_MASK )){
			selector++;
		}
if (currentDir==(KBD_LEFT&KBD_MASK )){
			selector--;
		}
if (currentDir==(KBD_SELECT) && selector == 0 ){
			  startAudio();
		}
if (currentDir==(KBD_SELECT) && selector == 1 ){
			  game();
		}
if (currentDir==(KBD_SELECT) && selector == 2 ){
				gallery();
		}

	}

}