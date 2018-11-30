
/*
   MIT License - Copyright (c) 2018 Francielle da Silva Nunes
   Criada em 24 nov 2018
*/

#include "FreeRTOS_AVR.h"

#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>

#define SS_PIN 53
#define RST_PIN 49
MFRC522 mfrc522(SS_PIN, RST_PIN);

#define RS_PIN 6
#define EN_PIN 7
#define D4_PIN 5
#define D5_PIN 4
#define D6_PIN 3
#define D7_PIN 2
LiquidCrystal lcd(RS_PIN, EN_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN);

#define LED_GREEN 24
#define LED_RED   22

/* A tarefa responsável pela leitura do leitor RFID e envio de mensagens para o gatekeeper stdio. */
static void rfidReaderTask( void *pvParameters );

/* A tarefa do gatekeeper em si. */
static void prvStdioGatekeeperTask( void *pvParameters );

/* Define as strings que serão impressas no LCD através do gatekeeper. */
static char *pcStringsToPrint[] =
{
  "Aproxime a tag..\n",
  "Acesso liberado!\n",
  "Acesso negado!\n"
};

/* Variável do tipo QueueHandle_t usada para enviar mensagens da tarefa do leitor para a tarefa do gatekeeper. */
QueueHandle_t xPrintQueue;

void setup( void )
{
  /* Inicia a porta serial e configura a taxa de dados para 9600 bps.  */
  Serial.begin(9600);

  /* Configura o número de colunas e linhas do display. */
  lcd.begin(16, 2);

  /* Predetermina os pinos digitais como saídas. */
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  /* Antes de uma fila ser usada, ela deve ser explicitamente criada.
    Neste caso, a fila é criada para conter no máximo 5 ponteiros de caracteres. */
  xPrintQueue = xQueueCreate( 5, sizeof( char * ) );

  /* Atraso pseudo-aleatório. */
  srand( 567 );

  /* Verifica se a fila foi criada com sucesso. */
  if ( xPrintQueue != NULL ) {
    /* Tarefa responsável pela leitura do leitor de RFID. */
    xTaskCreate( rfidReaderTask, "RFID Reader", 200, ( void * ) 1, 1, NULL );

    /* Crie a tarefa do gatekeeper. Esta é a única tarefa que tem permissão de escrita no LCD. */
    xTaskCreate( prvStdioGatekeeperTask, "Gatekeeper", 200, NULL, 0, NULL );

    /* Inicie o scheduler para que as tarefas criadas iniciem a execução. */
    vTaskStartScheduler();
  }

  /* Se tudo estiver bem, nunca chegaremos aqui, pois o scheduler agora estará
    executando as tarefas. Se chegarmos aqui, é provável que haja memória de
    heap insuficiente para que um recurso seja criado. */
  for ( ;; );
  //  return 0;
}
/*-----------------------------------------------------------*/

static void prvStdioGatekeeperTask( void *pvParameters )
{
  char *pcMessageToPrint;

  /* Esta é a única tarefa que pode gravar no LCD. Qualquer outra
    tarefa que queira gravar na saída não acessa o LCD diretamente,
    mas envia a saída para essa tarefa. Como apenas uma tarefa grava no
    LCD, não há problemas de exclusão ou serialização mútua a serem
    considerados dentro dessa tarefa. */
  for ( ;; )
  {
    /* Esperando uma mensagem chegar. */
    xQueueReceive( xPrintQueue, &pcMessageToPrint, portMAX_DELAY );

    /* Não há necessidade de verificar o valor de retorno, pois
      a tarefa será bloqueada indefinidamente e será executada
      somente quando a mensagem chegar. Quando a próxima linha é
      executada, haverá uma mensagem a ser emitida. */

    /* Função para exibição da mensagem no LCD. */
    displayMessage(pcMessageToPrint);

    /* Função para acender o LED correspondente a permissão de acesso. */
    turnOnLEDs(pcMessageToPrint);

    // Serial.print(pcMessageToPrint );
    // Serial.flush();
    if (Serial.available()) {
      vTaskEndScheduler();
    }
    /* Agora simplesmente volte para aguardar a próxima mensagem. */
  }
}

static void rfidReaderTask( void *pvParameters )
{
  (void) pvParameters;

  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  for ( ;; )
  {
    /* Procura por novos cartões. */
    if ( ! mfrc522.PICC_IsNewCardPresent()) {
      /* Imprima a string, não diretamente, mas passando a string para
        a tarefa do gatekeeper na fila. A fila é criada antes de o scheduler
        ser iniciado, portanto, já existirá no momento em que essa tarefa
        for executada. Um tempo de bloco não é especificado, pois sempre
        deve haver espaço na fila. */
      xQueueSendToBack( xPrintQueue, &( pcStringsToPrint[ 0 ] ), 0 );
    } else {
      /* Seleciona um dos cartões. */
      if ( ! mfrc522.PICC_ReadCardSerial()) {
        xQueueSendToBack( xPrintQueue, &( pcStringsToPrint[ 0 ] ), 0 );
      } else {
        /* Reconhece o UID da tag e verifica a permissão de acesso. */
        if (mfrc522.uid.uidByte[0] == 0x05 && mfrc522.uid.uidByte[1] == 0x5C && mfrc522.uid.uidByte[2] == 0xB0 && mfrc522.uid.uidByte[3] == 0x6B) {
          /* Coloca a mensagem na fila para ser exibida no LCD. */
          xQueueSendToBack( xPrintQueue, &( pcStringsToPrint[ 1 ] ), 0 );
        } else {
          xQueueSendToBack( xPrintQueue, &( pcStringsToPrint[ 2 ] ), 0 );
        }
      }
    }
    vTaskDelay( 1000 / portTICK_PERIOD_MS );
  }
}

void loop() {}

void displayMessage(char *pcMessageToPrint) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(pcMessageToPrint);
}

void turnOnLEDs(char *pcMessageToPrint) {
  if (pcMessageToPrint == "Acesso liberado!\n") {
    digitalWrite(LED_GREEN, HIGH);
    delay(1000);
    digitalWrite(LED_GREEN, LOW);
  } else if (pcMessageToPrint == "Acesso negado!\n") {
    digitalWrite(LED_RED, HIGH);
    delay(1000);
    digitalWrite(LED_RED, LOW);
  }
}
