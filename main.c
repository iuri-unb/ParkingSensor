#include <msp430.h>
#include <stdint.h>
#include "clock.h"
#include "pmm.h"

#define VERMELHO        0
#define VERDE           1
#define VERDE_VERMELHO  2

#define ACENDER         1
#define APAGAR          0

volatile unsigned int time_captured;

void config_pins(){
    // LED vermelho
    P1DIR |=  BIT0;                                         // Seta o pino P1.0 como saida
    P1OUT &= ~BIT0;                                         // P1.0 = 0

    // LED verde
    P4DIR |=  BIT7;                                         // Seta o pino P4.7 como saida
    P4OUT &= ~BIT7;                                         // P4.7 = 0

    // Trigger P1.5
    P1OUT &= ~BIT5;                                         // Flanco de descida do trigger caso ele esteja ligado
    P1DIR |=  BIT5;                                         // Trigger -> ConfiguraDO para saída P1.5

    // Echo P2.0
    P2DIR &= ~BIT0;                                         // Definido como entrada
    P2REN |=  BIT0;                                         // Habilitar resisitor
    P2OUT |=  BIT0;                                         // Liga resistor de pull up
    P2SEL |=  BIT0;                                         // Mode Capture Input Signal

    // Buzzer P2.5
    P2DIR |=  BIT5;                                         // Buzzer -> Configurado para saída P2.5
}

void set_led(int color, int status){
    if(color == 0 && status == 1){
        P1OUT |= BIT0;                                      // Acende o LED vermelho
        P4OUT &= ~BIT7;                                     // Apaga o LED verde
    }else if(color == 1 && status == 1){
        P4OUT |= BIT7;                                      // Acende o LED verde
        P1OUT &= ~BIT0;                                     // Apaga o LED vermelho
    }else if(color == 2 && status == 1){
        P1OUT |= BIT0;                                      // Acende o LED vermelho
        P4OUT |= BIT7;                                      // Acende o LED verde
    }else if(color == 2 && status == 0){
        P1OUT &= ~BIT0;                                     // Apaga o LED vermelho
        P4OUT &= ~BIT7;                                     // Apaga o LED verde
    }
}

void delay_us(uint16_t t_micro){
    TB0CTL = (TASSEL__SMCLK | MC__UP);                      // Timer -> SMCLK, UpMode.
    TB0CCTL0 = OUTMOD_1;                                    // Modo de saida para SET
    TBCCR0 = t_micro;                                       // Limiar da contagem

    TB0CTL |= TACLR;                                        // Reseta o timer e inicia a contagem

    while(!(TB0CCTL0 & CCIFG));                             // Enquanto o tempo for menor aguarda o delay
}

void set_timer_echo(){
    TA1CCTL1 = (CAP | CM_3 | CCIE | CCIS_0);                // Timer -> Capture Mode, Both Edges, Capture/compare interrupt enable, Capture input select: 0 - CCIxA
    TA1CTL   = (TASSEL__SMCLK | TACLR | MC__CONTINUOUS);    // Timer -> SMCLK, Clear, Continuous Mode
}

void send_trigger(unsigned int delay_t){
    P1OUT |=  BIT5;                                         // Emite o sinal do trigger
    delay_us(delay_t);                                      // Espera pelo tempo em us recebido em delay_t
    P1OUT &= ~BIT5;                                         // Para de emitir o sinal do trigger
}

void play_buzzer(unsigned int distance){
    volatile unsigned int freq_clk;
    freq_clk = (50)*(distance/58);

    TA2CCR0 = freq_clk;                                     // Coloca o timer escolhido
    TA2CCR2 = freq_clk/2;                                   // Gera uma saida em TA2.2 com 50% duty cycle
    TA2CTL  = TACLR | TASSEL__SMCLK |  MC__UP;              // Timer -> SMCLK, Up Mode, Clear
    TA2CCTL0 |= CCIE;                                       // Habilita a interrupcao CCR0
    TA2CCTL2 |= OUTMOD_7;                                   // Modo de saida para TA2.2

    if(distance > 0)                                        // Se nota for maior que 0
        P2SEL |= BIT5;                                      // Emite o sinal ataul no buzzer

    delay_us(500);
    __bis_SR_register(LPM0_bits + GIE);                     // Vai para LPM e depois para interrupção que ira sair do LPM
    TA2CTL = MC_0;                                          // Para o timer
}

void set_result(int diff_time){                             // Mostra o(s) LED(s) correspondentes e toca o buzzer na frequencia certa
    if (diff_time <= 580) {                                 // <= 10 cm
        set_led(VERDE_VERMELHO, ACENDER);                   // Acende o LED vermelho e verde
        play_buzzer(diff_time);                             // Envia o sinal ao buzzer
    }
    else if (diff_time > 580 && diff_time <= 1740) {        // 10 cm < x <= 30 cm
        set_led(VERMELHO, ACENDER);                         // Acende o LED vermelho
        play_buzzer(diff_time);                             // Envia o sinal ao buzzer
    }
    else if (diff_time > 1740 && diff_time <= 2900) {       // 30 cm < x <= 50 cm
        set_led(VERDE, ACENDER);                            // Acende o LED verde
        play_buzzer(diff_time);                             // Envia o sinal ao buzzer
    }else {
        set_led(VERDE_VERMELHO, APAGAR);                    // Apaga o LED vermelho e verde
        P2SEL &= ~BIT5;                                     // Desliga o buzzer
    }
}

#pragma vector = TIMER1_A1_VECTOR
__interrupt void TA1_CCRN_ISR(){                            // Rotina pra tratar as interrupcoes do echo
    switch (TA1IV) {                                        // Lendo TA1IV, limpa TAIFG
    case TA1IV_TACCR1:                                      // TA1CCR1
        if( TA1CCTL1 & CCI ){                               // Flanco de subida
            time_captured = TA1CCR1;                        // Salva o tempo do flanco de subida
        }
        else {
            time_captured = TA1CCR1 - time_captured;        // Diferenca entre o tempo atual e o anterior
            set_result(time_captured);                      // Chama a rotina que exibe o led correspondente e toca o buzzer
        }
        break;
    default: break;
    }
}

#pragma vector = TIMER2_A0_VECTOR                           // Timer 2 CCR0 Interrupt Vector
__interrupt void CCR0_ISR(void){
   __bic_SR_register_on_exit(LPM0_bits);                    // Sai do LPM
}

void main(){
    WDTCTL = WDTPW | WDTHOLD;                               // Stop Watchdog Timer

    // Increase core voltage so CPU can run at 16Mhz
    // Step 1 allows clocks up to 12MHz, step 2 allows rising MCLK to 16MHz
    pmmVCore(1);
    pmmVCore(2);

    // Configure clock
    // This should make MCLK @16MHz, SMCLK @1MHz and ACLK @32768Hz
    clockInit();

    config_pins();                                          // Chama a rotina para configurar os pinos

    __enable_interrupt();                                   // Habilita interrupcoes no codigo

    while(1){
        send_trigger(100);                                  // Manda o sinal do trigger por 100us
        set_timer_echo();                                   // Inicia o timer do echo
        delay_us(50000);                                    // Espera por 50000us = 50ms = 0,5s para ter no maximo 20 medicoes por segundo
    }
}
