#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>
#include <ESP8266TelegramBOT.h>
#include <PubSubClient.h>

const byte eepromValid = 123;    // If the first byte in eeprom is this thenv the data is valid.
 
/*Pin definitions*/
const int fechadura = 33;       // Pino de comando da fechadura solenoide (Ativando relé).
const int programButton = 19;  // Botao Manual Interno
const int lockPin = 27;        // LED Vermelho
const int ledPin = 22;         // LED Verde
const int knockSensor = 32;    // Piezo Elétrico

/*Tuning constants. Changing the values below changes the behavior of the device.*/
int threshold = 1500;                   // Minimum signal from the piezo to register as a knock. Higher = less sensitive. Typical values 1 - 10
const int rejectValue = 25;           // If an individual knock is off by this percentage of a knock we don't unlock. Typical values 10-30
const int averageRejectValue = 15;    // If the average timing of all the knocks is off by this percent we don't unlock. Typical values 5-20
const int knockFadeTime = 150;        // Milliseconds we allow a knock to fade before we listen for another one. (Debounce timer.)
const int lockOperateTime = 2500;     // Milliseconds that we operate the lock solenoid latch before releasing it.
const int maximumKnocks = 20;         // Maximum number of knocks to listen for.
const int knockComplete = 1200;       // Longest time to wait for a knock before we assume that it's finished. (milliseconds)
 
byte secretCode[maximumKnocks] = {50, 25, 25, 50, 100, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  // Initial setup: "Shave and a Hair Cut, two bits."
int knockReadings[maximumKnocks];    // When someone knocks this array fills with the delays between knocks.
int knockSensorValue = 0;            // Last reading of the knock sensor.
boolean programModeActive = false;   // True if we're trying to program a new knock.
boolean programModeActive_para_alterar = false;

int pressC = 0;

// Initialize Wifi connection to the router
//char ssid[] = "COC_LAN";              // your network SSID (name)
//char password[] = "copa2018@russia7x1";  // your network key     
char ssid[] = "wifi";              // your network SSID (name)
char password[] = "teste123";  // your network key     
const char* mqttServer = "192.168.0.220";
const int mqttPort = 1883;
const char* mqttUser = "adilson";
const char* mqttPassword = "12345678";

WiFiClientSecure espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// Initialize Telegram BOT
#define BOTtoken "564806573:AAHxTOv-AEsg-wwk-j52z01zuYlXpThbb8k"  //token of TestBOT
#define BOTname "PortaIOT_bot"
#define BOTusername "PortaIOT_Pelicano"
TelegramBOT bot(BOTtoken, BOTname, BOTusername);

int Bot_mtbs = 1000; //mean time between scan messages
long Bot_lasttime;   //last time messages' scan has been done

char keyboard_response[] = "Porta Aberta via Teclado";
char piezo_response[] = "Porta Aberta via Toque";
char button_response[] = "Porta Aberta via Botão Interno";
char telegram_response[] = "Porta Aberta via Telegram";
char mqtt_response[] = "Porta Aberta via MQTT";
char inTopic[] = "Porta_entrada";
char outTopic[] = "Porta_saida";
char chatId[] = "-304954782";


//Inicialização do Display I2C
LiquidCrystal_I2C lcd(0x3F,20,4); //

int count = 0;                                         // Contador de uso geral
char pass [4] = {'1', '2', '3', '4'};                 // Senha inicial do teclado
const byte ROWS = 4;                                 // Quatro linhas
const byte COLS = 4;                                // Três colunas

//Mapeamento de teclas
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {15, 2, 0, 4};                   //Definição de pinos das linhas
byte colPins[COLS] = {16, 17, 5, 18  };              //Definição de pinos das colunas


//Cria o teclado
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

void setup(){
  
setupBOT();

pinMode(programButton, INPUT_PULLUP);       //Define o pino do botão interno como entrada e habilita o resistor interno de Pull Up.
pinMode(ledPin, OUTPUT);                    //Define o pino do LED Verde como saída.
pinMode(lockPin, OUTPUT);                   //Define o pino do LED Vermelho como saída.
pinMode(fechadura, OUTPUT);                 //Define o pino de comando da fechadura como saída (Relé).
  
lcd.backlight();                            //Acende o Display.
readSecretKnock();                          // Load the secret knock (if any) from EEPROM.
doorUnlock(500);                            // Unlock the door for a bit when we power up. For system check and to allow a way in if the key is forgotten.
delay(500);                                 // This delay is here because the solenoid lock returning to place can otherwise trigger and inadvertent knock.
lcd.begin(12,14);                           //Inicializa o LCD
//Serial.begin(9600);                         //Inicializa a comunicação serial a 9600 bps

lcd.clear();                                //Limpa LCD
key_init();                                 //Função inicial do sistema
}



///// ///// ///// ///// /////   INICIO DO LOOP   ///// ///// ///// ///// ///// /////
void loop(){
digitalWrite(fechadura,HIGH);
int button_state_now = digitalRead(programButton);

if(button_state_now == LOW){
  manualUnlocked();
  //forceUnlocked();
  key_init();
}

loopBOT();

  
char key = keypad.getKey();                           //Obtém tecla pressionada
///// ///// ///// ///// /////   FUNÇÃO TECLA A   ///// ///// ///// ///// ///// /////

  if (key != NO_KEY){                            //Se foi pressionada uma tecla:
    
    if (key == 'A') {                            //Se a tecla pressionada for "A"
      code_entry_init();                         //Então espera que seja inserida uma senha
      int entrada = 0;
      int cursorAsterisco = 7;                   //Define o numero 5 para usar como posição na coluna do display e gerar o asterisco.
      while (count < 4 ){                        //Conta 4 entradas/teclas
        char key = keypad.getKey();              //Obtém tecla pressionada
        if (key != NO_KEY){                      //Se foi pressionada uma tecla:
          entrada += 1;                          //Faz entrada = entrada + 1
          cursorAsterisco += 1;                  //Conta mais um para a variavel, fazendo o cursor pular uma casa para gerar o próximo asterisco.
          lcd.setCursor(cursorAsterisco,2);      //seleciona o cursor na posição definida pela variável "cursorAsterisco".
          lcd.print("*");                        //imprime o asterisco na tela quando a senha é digitada.
          


          if (key == pass[count])count += 1;     //Se a tecla pressionada corresponde ao dígito da senha correspondente, soma 1 no contador
          
          if ( count == 4 ) {
            
            keyboardResponse();
            delay(2000);
            unlocked();          //Se contador chegou a 4 e com dígitos corretos, desbloqueia sistema
          }

          if ((entrada == 4) && (count < 4)){    //Se foi pressionada 4 teclas, mas o contador não chegou a 4, acesso negado.
                denied();
          }
          
          if ((key == 'A') || (entrada == 4)){   //Se foi pressionada a tecla "A' ou foram feitas 4 entradas
             locked(); 
             key_init();                         //Inicializa o sistema
            break;                               //Para o sistema e espera por uma tecla
      }
    }
  }
}

///// ///// ///// ///// /////   FUNÇÃO TECLA B   ///// ///// ///// ///// ///// /////

    if(key == 'B'){                             //Se a tecla pressionada for "B"
      old_pass_check();                         // mensagem para entrar a senha antiga
      int entrada = 0;
      int cursorAsterisco = 7;
      while (count < 4 ){
        char key = keypad.getKey();
        if (key != NO_KEY){
          entrada += 1;
          cursorAsterisco += 1;                 //Conta mais um para a variavel, fazendo o cursor pular uma casa para gerar o próximo asterisco.
          lcd.setCursor(cursorAsterisco,3);     //seleciona o cursor na posição definida pela variável "cursorAsterisco".
          lcd.print("*");                       //imprime o asterisco na tela quando a senha é digitada.
          if (key == pass[count])count += 1;
          if ( count == 4 ){                    // foi teclado a senha antiga corretamente
            get_new_pass();                     // chama função para entrada da nova senha
          }

          if ((entrada == 4) && (count < 4)){   //Se foi pressionada 4 teclas, mas o contador não chegou a 4, acesso negado.
                denied();
          }
          
          if ((key == 'B') || (entrada == 4)){  // Foi teclado B ou entrou 4 números errados
             key_init();                        // Inicializa
            break;                              // Interrompe loop
      }
    }
  }
}  

///// ///// ///// ///// /////   FUNÇÃO TECLA C   ///// ///// ///// ///// ///// /////

if (key == 'C'){                    //Se a tecla pressionada for "C"

    toque_entry_init();
    pressC = 0;

        while (pressC <= 0) {
          bate_porta();
          }
}

///// ///// ///// ///// /////   FUNÇÃO TECLA D   ///// ///// ///// ///// ///// /////

if (key == 'D') {                   //Se a tecla pressionada for "D"
  
    toque_entry_init();
    pressC = 0;

        while (pressC <= 0) {
          bate_porta();
          }
}

 
} //Final do "if (key != NO_KEY)"
}   //Final do Void Loop.

///// ///// ///// ///// /////   FINAL DO LOOP   ///// ///// ///// ///// ///// /////

//Subrotina para Trancado
void locked(){
  lcd.clear();                        //Limpa LCD
  delay(100);
}


void get_new_pass(){
    new_pass_entry(); // mensagem, som e LED
    int entrada = 0; // inicializa entrada
    int cursorAsterisco = 7;
    while(count < 4){ // enquanto contador for menor que 4
      char key = keypad.getKey(); // obtem informação do teclado
      if(key != NO_KEY){ // se algo foi teclado
        entrada += 1; // aumenta contador de entrada
          cursorAsterisco += 1;                     //Conta mais um para a variavel, fazendo o cursor pular uma casa para gerar o próximo asterisco.
          lcd.setCursor(cursorAsterisco,3);        //seleciona o cursor na posição definida pela variável "cursorAsterisco".
          lcd.print("*");                         //imprime o asterisco na tela quando a senha é digitada.
        pass[count] = key; // aramazena o novo dígito
        count += 1; // próximo dígito
        if(count == 4){
          lcd.clear();
          lcd.setCursor(3,1);
          lcd.print("Senha alterada!");
          delay(2000);
          break; // chegou a 4 digitos, interrompe loop     
        }
        if((key == 'B') || (entrada == 4)){// foi teclado B 4 entradas
          key_init();// inicializa sistema
          break; // sai
        }
      }
    }
}

void new_pass_entry(){
  count = 0;
  lcd.clear();                        //Limpa LCD
  lcd.setCursor(2,0);
  lcd.print("Alterar  senha");              //Emite mensagem
  lcd.setCursor(0,2);
  lcd.print("Digite a senha nova:");              //Emite mensagem
}

void old_pass_check(){
  lcd.clear();                        //Limpa LCD
  lcd.setCursor(1,0);
  lcd.print("Confirmar Usuario");              //Emite mensagem
  lcd.setCursor(1,2);
  lcd.print("Digite senha atual:");              //Emite mensagem
}

//Função de Inicializar o Sistema 
void key_init (){
  lcd.clear();                        //Limpa o LCD
  lcd.setCursor(0,0);
  lcd.print("A - Inserir senha");              //Emite mensagem
  lcd.setCursor(0,1);                 //Muda de linha
  lcd.print("B - Alterar senha");          //Emite mensagem
  lcd.setCursor(0,2);
  lcd.print("C - Inserir toque");              //Emite mensagem
  lcd.setCursor(0,3);
  lcd.print("D - Alterar toque");              //Emite mensagem
  count = 0;                          //Variável count é zero na inicialização
}

//Subrotina de Entrada da Senha 
void code_entry_init(){
  lcd.clear();                        //Limpa LCD
  lcd.setCursor(1,1);
  lcd.print("Insira a sua senha:");        //Emite mensagem
  count = 0;                          //Variável count é zero na entrada de senha
}

//Subrotina de Entrada de toque 
void toque_entry_init(){
  lcd.clear();                        //Limpa LCD
  lcd.print("Bata o codigo:");       //Emite mensagem
}

//Função de aesso liberado por senha ou batida.
void unlocked(){
  lcd.clear();                        //Limpa LCD
  lcd.setCursor(2,0);
  lcd.print("Seja bem-vindo!");      //Emite mensagem
  lcd.setCursor(2,2);
  lcd.print("Acesso Liberado");      //Emite mensagem
  digitalWrite (fechadura, LOW);                   //Destravar solenoide
  delay (3000);
  lcd.clear();
  digitalWrite (fechadura, HIGH); 
}

//Função de aesso liberado por senha ou batida.
void manualUnlocked(){
  lcd.clear();                        //Limpa LCD
  lcd.setCursor(2,1);
  lcd.print("Porta destravada");      //Emite mensagem
  lcd.setCursor(4,2);
  lcd.print("manualmente");      //Emite mensagem
  digitalWrite (fechadura, LOW);                   //Destravar solenoide
  delay (3000);
  lcd.clear();
  digitalWrite (fechadura, HIGH); 
  buttonResponse();
}

//Subrotina para Acesso Negado
void denied(){
  lcd.clear();                        //Limpa LCD
  lcd.setCursor(2,0);
  lcd.print("Senha incorreta");      //Emite mensagem
  lcd.setCursor(3,2);
  lcd.print("Acesso negado");      //Emite mensagem
  delay(2000);
}

//Função de toque na porta
int bate_porta(){
  // Listen for any knock at all.
  knockSensorValue = analogRead(knockSensor);

  // if (digitalRead(programButton) == HIGH){  // is the program button pressed?
  //   delay(100);   // Cheap debounce.
  //   if (digitalRead(programButton) == HIGH){ 
  //     if (programModeActive == false){     // If we're not in programming mode, turn it on.
  //       programModeActive = true;          // Remember we're in programming mode.
  //       digitalWrite(ledPin, HIGH);        // Turn on the red light too so the user knows we're programming.
  //     } else {                             // If we are in programing mode, turn it off.
  //       programModeActive = false;
  //       digitalWrite(ledPin, LOW);
  //       delay(500);
  //     }
  //     while (digitalRead(programButton) == HIGH){
  //       delay(10);                         // Hang around until the button is released.
  //     } 
  //   }
  //   delay(250);   // Another cheap debounce. Longer because releasing the button can sometimes be sensed as a knock.
  // }
  
  
  if (knockSensorValue >= threshold){
     if (programModeActive == true){  // Blink the LED when we sense a knock.
       digitalWrite(ledPin, LOW);
     } else {
       digitalWrite(ledPin, HIGH);
     }
     knockDelay();
     if (programModeActive == true){  // Un-blink the LED.
       digitalWrite(ledPin, HIGH);
     } else {
       digitalWrite(ledPin, LOW);
     }
     listenToSecretKnock();           // We have our first knock. Go and see what other knocks are in store...
  }
 
} 
 
// Records the timing of knocks.
void listenToSecretKnock(){
  int i = 0;
  // First reset the listening array.
  for (i=0; i < maximumKnocks; i++){
    knockReadings[i] = 0;
  }
  
  int currentKnockNumber = 0;               // Position counter for the array.
  int startTime = millis();                 // Reference for when this knock started.
  int now = millis();   
 
  do {                                      // Listen for the next knock or wait for it to timeout. 
    knockSensorValue = analogRead(knockSensor);
    if (knockSensorValue >= threshold){                   // Here's another knock. Save the time between knocks.
      now=millis();
      knockReadings[currentKnockNumber] = now - startTime;
      currentKnockNumber ++;                             
      startTime = now;          
 
       if (programModeActive==true){     // Blink the LED when we sense a knock.
         digitalWrite(ledPin, LOW);
       } else {
         digitalWrite(ledPin, HIGH);
       } 
       knockDelay();
       if (programModeActive == true){  // Un-blink the LED.
         digitalWrite(ledPin, HIGH);
       } else {
         digitalWrite(ledPin, LOW);
       }
    }
 
    now = millis();
  
    // Stop listening if there are too many knocks or there is too much time between knocks.
  } while ((now-startTime < knockComplete) && (currentKnockNumber < maximumKnocks));
  
  //we've got our knock recorded, lets see if it's valid
  if (programModeActive == false){           // Only do this if we're not recording a new knock.
    if (validateKnock() == true){
      doorUnlock(lockOperateTime); 
    } else {

      lcd.clear();
      lcd.print("Toque incorreto!");
      // knock is invalid. Blink the LED as a warning to others.
      for (i=0; i < 5; i++){          
        digitalWrite(ledPin, HIGH);
        delay(100);
        digitalWrite(ledPin, LOW);
        delay(100);
      }

      delay(2000);
      pressC++;
      key_init();

    }
  } else { // If we're in programming mode we still validate the lock because it makes some numbers we need, we just don't do anything with the return.
    validateKnock();
  }
}

// reads the secret knock from EEPROM. (if any.)
void readSecretKnock(){
  byte reading;
  int i;
  reading = EEPROM.read(0);
  if (reading == eepromValid){    // only read EEPROM if the signature byte is correct.
    for (int i=0; i < maximumKnocks ;i++){
      secretCode[i] =  EEPROM.read(i+1);
    }
  }
}

// Unlocks the door.
void doorUnlock(int delayTime){

  lcd.clear();
  lcd.print("Toque correto!");
  digitalWrite(fechadura, LOW);
  
  // knock is invalid. Blink the LED as a warning to others.
      for (int i=0; i < 5; i++){          
        digitalWrite(lockPin, HIGH);
        delay(100);
        digitalWrite(lockPin, LOW);
        delay(100);
      }
  
  delay(5000);
  digitalWrite(fechadura, HIGH);
  piezoResponse();
  pressC++;
  key_init();
}

// Plays a non-musical tone on the piezo.
// playTime = milliseconds to play the tone
// delayTime = time in microseconds between ticks. (smaller=higher pitch tone.)
void chirp(int playTime, int delayTime){
  long loopTime = (playTime * 1000L) / delayTime;
  for(int i=0; i < loopTime; i++){
    delayMicroseconds(delayTime);
  }
}

// Deals with the knock delay thingy.
void knockDelay(){
  int itterations = (knockFadeTime / 20);      // Wait for the peak to dissipate before listening to next one.
  for (int i=0; i < itterations; i++){
    delay(10);
    analogRead(knockSensor);                  // This is done in an attempt to defuse the analog sensor's capacitor that will give false readings on high impedance sensors.
    delay(10);
  } 
}

// Checks to see if our knock matches the secret.
// Returns true if it's a good knock, false if it's not.
boolean validateKnock(){
  int i = 0;
 
  int currentKnockCount = 0;
  int secretKnockCount = 0;
  int maxKnockInterval = 0;               // We use this later to normalize the times.
  
  for (i=0;i<maximumKnocks;i++){
    if (knockReadings[i] > 0){
      currentKnockCount++;
    }
    if (secretCode[i] > 0){         
      secretKnockCount++;
    }
    
    if (knockReadings[i] > maxKnockInterval){   // Collect normalization data while we're looping.
      maxKnockInterval = knockReadings[i];
    }
  }
  
  // If we're recording a new knock, save the info and get out of here.
  if (programModeActive == true){
      for (i=0; i < maximumKnocks; i++){ // Normalize the time between knocks. (the longest time = 100)
        secretCode[i] = map(knockReadings[i], 0, maxKnockInterval, 0, 100); 
      }
      saveSecretKnock();                // save the result to EEPROM
      programModeActive = false;
      playbackKnock(maxKnockInterval);
      return false;
  }
  
  if (currentKnockCount != secretKnockCount){  // Easiest check first. If the number of knocks is wrong, don't unlock.
    return false;
  }
  
  /*  Now we compare the relative intervals of our knocks, not the absolute time between them.
      (ie: if you do the same pattern slow or fast it should still open the door.)
      This makes it less picky, which while making it less secure can also make it
      less of a pain to use if you're tempo is a little slow or fast. 
  */
  int totaltimeDifferences = 0;
  int timeDiff = 0;
  for (i=0; i < maximumKnocks; i++){    // Normalize the times
    knockReadings[i]= map(knockReadings[i], 0, maxKnockInterval, 0, 100);      
    timeDiff = abs(knockReadings[i] - secretCode[i]);
    if (timeDiff > rejectValue){        // Individual value too far out of whack. No access for this knock!
      return false;
    }
    totaltimeDifferences += timeDiff;
  }
  // It can also fail if the whole thing is too inaccurate.
  if (totaltimeDifferences / secretKnockCount > averageRejectValue){
    return false; 
  }
  
  return true;
}

//saves a new pattern too eeprom
void saveSecretKnock(){
  EEPROM.write(0, 0);  // clear out the signature. That way we know if we didn't finish the write successfully.
  for (int i=0; i < maximumKnocks; i++){
    EEPROM.write(i+1, secretCode[i]);
  }
  EEPROM.write(0, eepromValid);  // all good. Write the signature so we'll know it's all good.
}

// Plays back the pattern of the knock in blinks and beeps
void playbackKnock(int maxKnockInterval){


lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("Toque alterado");
      lcd.setCursor(2,1);
      lcd.print("com sucesso!");

      digitalWrite(ledPin, LOW);
      delay(1000);
      digitalWrite(ledPin, HIGH);
      chirp(200, 1800);
      for (int i = 0; i < maximumKnocks ; i++){
        digitalWrite(ledPin, LOW);
        // only turn it on if there's a delay
        if (secretCode[i] > 0){                                   
          delay(map(secretCode[i], 0, 100, 0, maxKnockInterval)); // Expand the time back out to what it was. Roughly. 
          digitalWrite(ledPin, HIGH);
          chirp(200, 1800);
        }
      }
      digitalWrite(ledPin, LOW);
      delay(1000);
      pressC++;
      key_init();
}

void Bot_EchoMessages() {

  for (int i = 1; i < bot.message[0][0].toInt() + 1; i++){
    if((bot.message[i][5] == "/abrir@PortaIOT_bot") || (bot.message[i][5] == "/abrir") ){
      telegramResponse();
      unlocked();
      key_init();
  }

  else if((bot.message[i][5] == "/start@PortaIOT_bot please") || (bot.message[i][5] == "/start")){
      bot.sendMessage(bot.message[i][4], "BEM VINDO A SUA PORTA IOT", "");
      bot.sendMessage(bot.message[i][4], "Digite /abrir para abrir sua porta", "");
      bot.sendMessage(bot.message[i][4], "Digite /ola para receber um olá", "");
  }
 }

  bot.message[0][0] = "";   // All messages have been replied - reset new messages
}

void setupBOT() {
  Serial.begin(115200);
  delay(3000);
  
  // attempt to connect to Wifi network:
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

//  client.setServer(mqttServer, mqttPort);
//
//  while (!client.connected()) {
//    Serial.println("Connecting to MQTT...");
//
//    if (client.connect("ESP32Client", mqttUser, mqttPassword )) {
//
//      Serial.println("connected");
//
//    } else {
//
//      Serial.print("failed with state");
//      Serial.print(client.state());
//      delay(2000);
//
//    }
//  }

  bot.begin();      // launch Bot functionalities
}

void loopBOT() {

  if (millis() > Bot_lasttime + Bot_mtbs)  {
    bot.getUpdates(bot.message[0][1]);   // launch API GetUpdates up to xxx message
    Bot_EchoMessages();   // reply to message with Echo
    Bot_lasttime = millis();
  }
}


void keyboardResponse(){
  // Telegram Response
  bot.sendMessage(chatId, keyboard_response, "");
  Serial.print("TAMO AQUI");

  //MQTT Response
  client.publish(outTopic, keyboard_response);
}

void piezoResponse(){
  // Telegram Response
  bot.sendMessage(chatId, piezo_response, "");

  //MQTT Response
  client.publish(outTopic, piezo_response);
}

void buttonResponse(){
  // Telegram Response
  bot.sendMessage(chatId, button_response, "");

  //MQTT Response
  client.publish(outTopic, button_response);
}

void telegramResponse(){
  // Telegram Response
  bot.sendMessage(chatId, telegram_response, "");

  //MQTT Response
  client.publish(outTopic ,telegram_response);

}

void mqttResponse(){
  // Telegram Response
  bot.sendMessage(chatId, mqtt_response, "");

  //MQTT Response
  client.publish(outTopic, mqtt_response);

}
