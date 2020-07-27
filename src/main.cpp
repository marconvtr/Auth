#include <Arduino.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <brzo_i2c.h>
#include "SSD1306Brzo.h"
#include "OLEDDisplayUi.h"
extern "C"{

#include "user_interface.h"

}


//#include "valida.cpp"
#include "images.h"


#define   CONTRAST_PIN   9
#define   BACKLIGHT_PIN  7
#define   CONTRAST       110


#define RST_PIN D4
#define SS_PIN D8




//////////////////
///// Config Wifi

int frame = 0;

const char* ssid     = "LARM";
const char* password = "UFSC2017larm";
const char* host = "150.162.234.150";  // Exemplo apenas


float sensor1 = 50;
float sensor2 = 80;
float sensor3 = 90;


////////////////////


/////Config TIMER

os_timer_t mTimer;
 
bool       _timeout = false;
 

////////////////

////// CONFIG MYQL

char REQ_SQL[] = "select * from reserva";
IPAddress server_addr(150,162,234,150);   // IP of the MySQL server
char user[] = "root";                     // MySQL user login username
char passwd[] = "";                 // MySQL user login password
 

/////////////////////////////



////// Config interrupcoes e contadores

const byte interruptPin = 13;
volatile byte interruptCounter = 0;
int numberofInterrupts = 0;
int timeoutCounter = 0;
int leu = 0;

long int wifiTry = 0;

enum estados{ONLINE,OFFLINE}; 
enum estados estadoAtual;
 
enum telas{NADA ,BOOT, APROXIME , CONECTANDO,  FALHA , AUTORIZADO , NAO_AUTORIZADO, SHOW_ID, GET, AUTENTICANDO};
enum telas telaAtual = NADA;
///////////////////


//// Funcoes

String RFIDcheck(void);
void tCallback(void *tCall);
int WifiGET(String ID);

void tCallback(void *tCall);
void usrInit(void);
void abrePorta(void);
uint SQLquery(uint id);

////Telas usuario
void lcdHello(void);
void lcdConnect(void);
void LcdWork(void);
void lcdOK(void);
void lcdGET(void);
void lcdAuth(void);
////Submenus Persistentes
void drawBarras(void);
void drawStats(void);
void drawAnim(void);
void topClean(void);
///// BITMAPS MEMORIA ROM (economizar RAM)  
/// tao no include images.h

////////////////// Fim bitmaps 



//// SETUP 


//Classe mfrc
MFRC522 mfrc522(SS_PIN, RST_PIN);

//classes e arquivos globais do displa
SSD1306Brzo display(0x3c, D3, D2);

OLEDDisplayUi ui     ( &display );

// how many frames are there?

// Overlays are statically drawn on top of a frame eg. a clock





void setup() {
    ///abrir seral e mandar nada

    Serial.begin(9600);
    Serial.println();

    ///habilitar interrupcao
    pinMode(interruptPin, INPUT_PULLUP);
    usrInit();

    ///habilitar SPI
    SPI.begin();
    delay(10);
    SPI.setFrequency(4000000);

    //Inicializar OLED
   
    ui.disableAllIndicators();
    // Initialising the UI will init the display too.
    ui.init();

    display.flipScreenVertically();
    //incializar RFID
    mfrc522.PCD_Init();
    ////Inicializar WIFI

    LcdWork();
    frame = 0;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED || wifiTry > 3333) {
        
        drawAnim();
        delay(3);

        wifiTry++;
    }
    topClean();
    display.clear();
    display.display();
    wifiTry = 0; 
    ////Modo funcionamento offline caso wifi nao esteja disponivel
    if(WiFi.status() != WL_CONNECTED){
        estadoAtual = OFFLINE;
        Serial.println("OFFLINE");
        drawStats();

    }
    else{
        estadoAtual = ONLINE;
        Serial.println("ONLINE");
        drawStats();
    }


    Serial.println(WiFi.localIP());
    void usrInit(void);
}

///// MAIN LOOOOPP @@@@@@@

void loop() {
    
    String ID;
    lcdHello();
    if (_timeout){

        ///Passou 1 segundo, fazer algo
        timeoutCounter++;
        leu = 0;
        _timeout = false;
    }
    //// Ta na hora de ler?
    if(leu == 0){
    ID = RFIDcheck();
    leu = 1;
    drawStats();
    }
    if(ID.toInt() != 0){

        int resultado;
        resultado = WifiGET(ID);
        ///TudoCorreto
        abrePorta();
    }

    ///// Passou 5 segundos e nada  aconteceu, o que fazer?
    if(timeoutCounter == 5 ){

        timeoutCounter = 0;
    }
}


String RFIDcheck(void){
    // Procura por cartao RFID
    if ( ! mfrc522.PICC_IsNewCardPresent()) 
    {
      //// CADE O CARTAO? 
    }
    // Seleciona o cartao RFID e le
    if ( ! mfrc522.PICC_ReadCardSerial()) 
    {
        //// bora entao
    }
    //Mostra UID na serial
    Serial.print("UID:");


    String conteudo= "";
    /// percorre matriz de bytes
    for (byte i = 0; i < mfrc522.uid.size; i++) 
    {   /// menor que 10? printa so numeros, maior que dez imprime HEX
       // Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
       // Serial.print(mfrc522.uid.uidByte[i], HEX);
        conteudo.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
        conteudo.concat(String(mfrc522.uid.uidByte[i], HEX));
    }
    if(conteudo.toInt() == 0)
        Serial.println("Sem Tag");
    else{
        conteudo.toUpperCase();
        Serial.println(conteudo);
    }
    /// Retorna o conteudo da TAG
    return conteudo;

}

int WifiGET(String ID){
    
    WiFiClient client;
    lcdAuth();
    drawStats();  
    const int httpPort = 8080;
    int counter = 0;
    if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    /// erro 2 nao conecta ao  servidor, servidor offline
    return 2;
  }
    String url = "/ReservaSala/pesquisaesp.php?sala=102&bloco=A"; 

    Serial.print("Requesting URL: ");
    Serial.println(url);

    // ENVIA A REQUISIÇÃO HTTP AO SERVIDOR 
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "Connection: close\r\n\r\n");
    frame = 0;
    lcdGET();
    drawStats();
    while(client.available() == 0 && counter < 5) {
        
        ///barrinha de loading
        drawAnim();
        if(_timeout){
            counter++;
        }
        
    }
    lcdAuth();
    drawStats();
    if(!client.available()){

        //conectado mas nao recebe request
        return 3;
    }
    lcdAuth();
    drawStats();
    /// conectou? show tratar dados agora
    while (client.available()) {

        //Adquire linha do DB
        String line = client.readStringUntil('\r');
        Serial.print(line);
        //Coleta primeiro campo (MAC ID RFID) tokenizado por ¨;¨
        String IDb = line.substring(0,line.indexOf(";"));
        //compara string
        //String igual
        IDb.toUpperCase();
        if(ID.compareTo(IDb) == 0)
        {
            ///autorizado
            return 1;   
        }

    }
    ///Usuario não encontrado
    return 0;
}




void tCallback(void *tCall){
    _timeout = true;
}
 
void usrInit(void){
    os_timer_setfn(&mTimer, tCallback, NULL);
    os_timer_arm(&mTimer, 1000, true);
}

void abrePorta(void){


}



void LcdWork(void){

    if(telaAtual != CONECTANDO){
        display.setFont(ArialMT_Plain_24);
        display.drawString(0,16,"Conectando");
        display.display();
    telaAtual = CONECTANDO;
    }
}

void lcdHello(void){
    display.setColor(WHITE);
    if(telaAtual != BOOT){
        display.clear();
        display.display();
        display.setFont(ArialMT_Plain_24);
        display.drawString(0,16,"Insira ID");
        display.display();
    telaAtual = BOOT;
    }
}



void lcdOK(void){
    if(telaAtual != AUTORIZADO){
        display.clear();
        display.display();
        display.setFont(ArialMT_Plain_24);
        display.drawString(0,16,"Autorizado");
        display.display();
        display.setFont(ArialMT_Plain_24);
        display.drawString(0,36,"Pessoa");
        display.display();
    telaAtual = AUTORIZADO;
    }

}

void lcdGET(void){

    if(telaAtual != GET){
    display.clear();
    display.display();
    display.setFont(ArialMT_Plain_24);
    display.drawString(0,16,"Conectando");
    display.display();
    display.setFont(ArialMT_Plain_24);
    display.drawString(0,36,"Servidor");
    display.display();
    telaAtual = GET ;
    }   

}

void lcdAuth(void){

    if(telaAtual != AUTENTICANDO){
    display.clear();
    display.display();
    display.setFont(ArialMT_Plain_24);
    display.drawString(0,16,"Analise");
    display.display();
    display.setFont(ArialMT_Plain_24);
    display.drawString(0,36,"ID");
    display.display();
    telaAtual = AUTENTICANDO ;
    }   

}




void drawAnim(void)
{
    if(frame < 127)
    {
        display.setColor(WHITE);
        display.drawLine(frame,15,frame,0);
        display.display();
        frame++;
    }else if(frame >= 127 && frame <= 254){
        display.setColor(BLACK);
        display.drawLine(255 - frame,15, 255 - frame,0);
        display.display();
        frame++;        
    }else if(frame > 254){
        frame = 0;
    }
    display.setColor(WHITE);

}

void drawStats(void){
    display.setColor(WHITE);
    if(WiFi.status() == WL_CONNECTED){
        display.setFont(ArialMT_Plain_10);
        display.drawString(0,50,"online");
        display.display();
        drawBarras();
    }else{
        display.setFont(ArialMT_Plain_10);
        display.drawString(0,53,"offline");
        display.display();  
    }
}

int potenciaSinal(void){
    
    // 5. High quality: 90% ~= -55db
    // 4. Good quality: 75% ~= -65db
    // 3. Medium quality: 50% ~= -75db
    // 2. Low quality: 30% ~= -85db
    // 1. Unusable quality: 8% ~= -96db
    // 0. No signal
    long rssi = WiFi.RSSI();
    int bars;
    
    if (rssi > -55) { 
        bars = 5;
    } else if (rssi < -55 && rssi > -65) {
        bars = 4;
    } else if (rssi < -65 && rssi > -75) {
        bars = 3;
    } else if (rssi < -75 && rssi > -85) {
        bars = 2;
    } else if (rssi < -85 && rssi > -96) {
        bars = 1;
    } else {
        bars = 0;
    }
    return bars;

}

void drawBarras(void){

    int nivel = potenciaSinal();
    int i = 0;
    
    display.setColor(BLACK);
    display.fillRect(63,50,64,14);
    display.display();
    display.setColor(WHITE);
    for(i = 0; i < nivel ; i++ ){
        display.fillRect(70 +(i*6),63 -(i*3),4,(i*3));
        display.display();
    }

}

void topClean(void){

    display.setColor(BLACK);
    display.fillRect(0,0,127,15);
    display.display();
}