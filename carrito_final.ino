#include <SoftwareSerial.h>

SoftwareSerial bluetooth(2, 9);  // RX=2, TX=9 (sin conectar)

// Ultrasónico
int Trig = 8;
int Eco = 7;
float tiempo, distancia;

// Pines de motores
int n1 = 3;
int n2 = 4;
int n3 = 12;
int n4 = 13;

// Sensores infrarrojos
int sensorIzq = 10;  // infrarrojo izquierdo
int sensorDer = 11;  // infrarrojo derecho

// Lo que lee el sensor sobre la línea negra (si va al revés, cámbialo a LOW)
const int LINEA = HIGH;

// false = "ambos en línea" avanza (sensores sobre la línea)
// true  = "ambos en línea" es el fin del circuito (se detiene)
const bool FIN_CON_AMBOS = false;

// Modos de funcionamiento
const int ESPERA = 0;  // parado, esperando el primer comando BT
const int MANUAL = 1;  // controlado desde el celular
const int AUTO = 2;    // sigue la línea con los infrarrojos
int modo = ESPERA;

const unsigned long TIEMPO_AUTO = 5000;  // 5 s sin comandos -> AUTO
unsigned long ultimoComando = 0;
unsigned long ultimaMedicion = 0;
unsigned long ultimoPrint = 0;

char comando = 'S';  // último comando del celular (S = parado)

void setup() {
  pinMode(Trig, OUTPUT);
  pinMode(Eco, INPUT);
  pinMode(sensorIzq, INPUT);
  pinMode(sensorDer, INPUT);
  pinMode(n1, OUTPUT);
  pinMode(n2, OUTPUT);
  pinMode(n3, OUTPUT);
  pinMode(n4, OUTPUT);
  Serial.begin(9600);
  bluetooth.begin(9600);
  randomSeed(analogRead(A0));  // para elegir el lado del giro al azar
  parar();                     // no se mueve hasta recibir un comando
}

void parar() {
  digitalWrite(n1, LOW);
  digitalWrite(n2, LOW);
  digitalWrite(n3, LOW);
  digitalWrite(n4, LOW);
}

void retroceder() {
  digitalWrite(n1, LOW);
  digitalWrite(n2, HIGH);
  digitalWrite(n3, HIGH);
  digitalWrite(n4, LOW);
}

void avanzar() {
  digitalWrite(n1, HIGH);
  digitalWrite(n2, LOW);
  digitalWrite(n3, LOW);
  digitalWrite(n4, HIGH);
}

void girarIzquierda() {
  digitalWrite(n1, LOW);  // motor A adelante
  digitalWrite(n2, LOW);
  digitalWrite(n3, LOW);  // motor B parado
  digitalWrite(n4, HIGH);
}

void girarDerecha() {
  digitalWrite(n1, HIGH);  // motor A parado
  digitalWrite(n2, LOW);
  digitalWrite(n3, LOW);  // motor B adelante
  digitalWrite(n4, LOW);
}

void avancePulsos() {
  avanzar();
  delay(20);
  parar();
  delay(20);
}

float medirDistancia() {
  digitalWrite(Trig, LOW);
  delay(1);
  digitalWrite(Trig, HIGH);
  delay(1);
  digitalWrite(Trig, LOW);

  tiempo = pulseIn(Eco, HIGH, 4000);  // timeout 4 ms (~68 cm)
  distancia = tiempo / 58.22;         // 0 si no hay eco
  return distancia;
}

void evadirObstaculo() {
  parar();
  delay(1000);  // se detiene

  retroceder();
  delay(1000);  // retrocede

  parar();
  delay(300);

  if (random(2) == 0) {  // gira a cualquier lado
    girarIzquierda();
  } else {
    girarDerecha();
  }
  delay(1500);

  parar();
  delay(300);

  // descarta los comandos que llegaron mientras evadía
  while (bluetooth.available()) {
    bluetooth.read();
  }

  if (modo == MANUAL) {
    comando = 'S';             // queda parado hasta nuevo comando
    ultimoComando = millis();  // 5 s para retomar el control
  }
  // en AUTO sigue en AUTO y vuelve a buscar la línea
}

void seguirLinea() {
  bool izq = (digitalRead(sensorIzq) == LINEA);
  bool der = (digitalRead(sensorDer) == LINEA);

  if (millis() - ultimoPrint >= 200) {  // imprime sin frenar el carro
    ultimoPrint = millis();
    Serial.print("Izq: ");
    Serial.print(izq);
    Serial.print("  Der: ");
    Serial.println(der);
  }

  if (!izq && !der) {  // ningún sensor ve la línea
    avanzar();
  } else if (!izq && der) {  // solo el derecho ve la línea
    girarDerecha();
  } else if (izq && !der) {  // solo el izquierdo ve la línea
    girarIzquierda();
  } else {  // ambos ven la línea
    avanzar();
  }
}

void loop() {
  // 1. Leer comando del celular
  if (bluetooth.available()) {
    char c = bluetooth.read();

    if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S') {
      Serial.println(c);
      if (modo != MANUAL) {
        Serial.println("Modo manual");
      }
      modo = MANUAL;             // cualquier comando desactiva los infrarrojos
      ultimoComando = millis();  // reinicia el contador de 5 s
      comando = c;

      switch (c) {
        case 'F': avanzar(); break;
        case 'B': retroceder(); break;
        case 'L': girarIzquierda(); break;
        case 'R': girarDerecha(); break;
        case 'S': parar(); break;
      }
    }
  }

  // 2. 5 s sin comandos: pasa a modo automático
  if (modo == MANUAL && millis() - ultimoComando >= TIEMPO_AUTO) {
    modo = AUTO;
    parar();
    Serial.println("Modo automatico");
  }

  // 3. Revisar obstáculo (en AUTO siempre; en MANUAL solo si avanza o gira)
  bool moviendose = (modo == AUTO) || (modo == MANUAL && (comando == 'F' || comando == 'L' || comando == 'R'));

  if (moviendose && millis() - ultimaMedicion >= 50) {
    ultimaMedicion = millis();
    medirDistancia();

    if (tiempo > 0 && distancia <= 15) {
      Serial.print(distancia);
      Serial.println(" cm - obstáculo");
      evadirObstaculo();  // bloquea el control hasta terminar
      return;
    }
  }

  // 4. Seguir la línea solo en modo automático
  if (modo == AUTO) {
    seguirLinea();
  }
}