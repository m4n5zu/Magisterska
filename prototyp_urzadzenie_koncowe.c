/* Oprogramowanie węzła końcowego 
Krzysztof Mansz Pomiary Technologiczne i Biomedyczne
Budowa stanowiska laboratoryjnego Zigbee - praca magisterska */

#include "framework.h"
#include "adc.h"
#include "leds.h"

#define END_POINT  1 //punkt końcowy dla transmisji
#define DATA_HANDLE 1 // obsluga ramki danych

// diody led w odpowiedzi na działania
#define NETWORK_LED  0 //dioda oznaczająca siec
#define NETWORK_TRANSMISSION_LED 1 //dioda oznaczająca transmisję danych

static IEEEAddr_t macAddr = 2; // adres MAC węzła
#define LED_FLASH_DELAY 300u; //określa miganie diody kiedy urządzenie szuka sieci

// mozliwe stany sieci
static enum 
{
	NETWORK_IDLE_STATE, //czeka na przycisk do naciśnięcia - trzeba przerobić bo inaczej na moim wezle niz na plytkach MeshBean
	NETWORK_JOINED_STATE, // procedura przylaczenia zakonczona
	NETWORK_JOIN_REQUEST_STATE; //stan żądania dołączenia
} networkState= NETWORK_IDLE_STATE; //stan domyślny

//stany dla miernika baterii
static enum
{
	SENSOR_OFF;
	SENSOR_READY;
	SENSOR_READ;
} sensorState= SENSOR_OFF; 

/*specjalne zastosowanie dla sensora ADC  - pumyśleć nad zastosowaniem dla pomiaru napięcia bateryjnego
short sensorUp= FALSE;
short sample = 0;
float sensorValue =0.0;
void processData(uint16_t);
static uint8_t temp_buffer[32]; */

// Prototypy funkcji
void RegisterNetworkEvents (void);
void SetNetworkParameters (void);
void SetPowerParameters (void);
void mainLoop();
void NetworkJoin (void);
void NetworkLost (void);
void NetworkTransmit (void);
void dataConfirm (uint8_t, FW_DataStatus_t);
void dataIndication (const FW_DataIndication_t *params);

// Funkcja przekazania kontroli dla użytkownika
void fw_userEntry (FW_ResetReason_t resetReason)  //resetReason wartosci mozliwe: 0- wykonanywany zimny reset, 1- zimny reset wystąpił, 2- Watchdog spowodowal reset
{
	
	TOSH_interrupt_enable(); //wlaczenie przerwan
	leds_open(); //ustawienie diód
	SetNetworkParameters(); //ustawienie parametrów sieci
	RegisterNetworkEvents(); //rejestracja zdarzeń sieciowych
	fw_registerEndPoint(END_POINT, dataIndication); //rejestracja punktu końcowego do transmisji i odbioru- 
	adc_init(); //inicjalizacja przetwornika ADC
	if(adc_open(ADC_INPUT_3, processData)==SUCCESS) //NIE zakonczy się sukcesem gdy kanał ADC jest już otwarty lub adcNumber (ADC_INPUT_3) jest poza zakresem
		sensorState=ready;
	//zacznij pętlę główną:
	fw_setUserLoop(200, mainLoop); //pętla główna użytkownika wywoływana periodycznie- jeśli pierwszy parametr jest zerem wtedy nie ma odstępu między wywolaniami
}

// Pętla główna
void Mainloop()
{
		switch(networkState)
		{
		// w sieci
		case NETWORK_JOINED_STATE: //jesli dołaczono
		{
			if(sensorState == SENSOR_READY) {
				adc_get (ADC_INPUT_3);
			} else if (sensorState == SENSOR_READ) {
				networkTransmit (); //wyslij dane koordynatorowi
				sensorState=SENSOR_READY; //zmien stan 
			}
			break;
		case NETWORK_IDLE_STATE: //siec jeszcze się nie zawiązała
			{
				// zawiąż sieć
				networkState= NETWORK_JOIN_REQUEST_STATE; //stan żądanie dostępu
				fw_joinNetwork(); //dolacz do sieci- nawiązuje dołączenie do sieci, gdy jest koordynatorem, jeśli nie dołącza do sieci
				break
			}
		case NETWORK_JOIN_REQUEST_STATE: //stan oczekiwania na dołączenie
			{
			static uint32_t ledTime =0;
			if((fw_getSystemTime() - ledTime ) > LED_FLASH_DELAY)
				{
					leds_toggle(NETWORK_LED);
					ledTime= fw_getSystemTime(); //czas systemowy w milisekundach
				}
		default:
			break;
		}
}
// funkcja zwrotna wywoływana przez adc_open:
void processData (uint16_t raw)
{
  /*tu kod dla przetwornika ADC który zapisze wartości zwrócone przez przetwornik np do postaci float */
  
}

/* definicje prototypów funkcji  z linijki 43 */
void networkJoin (void)
{
	//wskazanie dolaczenia do sieci
	leds_on(NETWORK_LED); //wlacz diodę okreslającą, że urządzenie dołączyło do sieci
	networkState= NETWORK_JOINED_STATE;
}

/* utrata sieci */
void networkLost (void)
{
	//oznacza opuszczenie sieci
	if (networkState != NETWORK_JOIN_REQUEST_STATE)
		leds_off(NETWORK_LED);
	networkState= NETWORK_JOIN_REQUEST_STATE;
}

void RegisterNetworkEvents()
{
	//rejestracja zdarzeń sieciowych
	FW_NetworkEvents_t handlers; // mozliwe zdarzenia dolaczenie, opuszczenie, nowe dziecko dodane , dziecko usunięte
	handlers.joined = networkJoin; // wezel dolaczyl do sieci
	handlers.lost = networkLost; //wezel opuscil siec
	handlers.addNode = NULL; //dodaj węzeł dziecko
	handlers.deleteNode = NULL; //usuń węzeł dziecko
	fw_registerNetworkEvents(&handlers); //rejestracja zdarzeń sieciowych
}

//ustawienie parametrow sieciowych
void SetNetworkParameters()
{
	FW_Param_t param;
	
	// ustaw rolę węzła 
	param.id = FW_NODE_ROLE_PARAM_ID;
	param.value.role= ZIGBEE_END_DEVICE_TYPE;
	fw_setParam(&param); //ustawia parametry Ezeenet
	
	// ustawienie adresu MAC
	param.id= FW_MAC_ADDR_PARAM_ID;
	macAddr=2;
	param.value.macAddr= &macAddr;
	fw_setParam(&param);
	
	// ustawienie PANID
	param.id= FW_PANID_PARAM_ID;
	param.value.panID= PANID;
	fw_setParam(&param);
	
	// Ustawienie maski kanału
	param.id= FW_CHANNEL_MASK_PARAM_ID;
	param.value.channelMask= CHANNEL_MASK;
	fw_setParam(&param);
	
	//ustawienie adresu logicznego:
	param.id= FW_NODE_LOGICAL_ADDR_PARAM_ID;
	param.value.logicalAddr =1;
	fw_setParam(&param);
}

// Transmisja danych po sieci
void networkTransmit(void)
{
	//inicjalizacja struktury danych
	FW_DataRequest_t params; 
	params.dstNWKAddr= 0; //koordynator- adres odbiorcy
	params.addrMode= NODE_NWK_ADDR_MODE;
	params.srcEndPoint= END_POINT;
	params.dstEndPoint= END_POINTl
	params.arq= TRUE;
	params.broadcast= FALSE;
	params.handle = DATA_HANDLE;
	params.data= temp_buffer; //okreslony linijka 40
	params.length=32;
	
	if(fw_dataRequest(&params, dataConfirm) ==FAIL) //FAIL- gdy transmisja danych za dlugo, 
		leds_off(NETWORK_TRANSMISSION_LED);
	else
		leds_on(NETWORK_TRANSMISSION_LED);
}

void dataConfirm (uint8_t handle, FW_DataStatus_t status)
{
	//wyczysc leda oznaczającego transmisję danych
	leds_off(NETWORK_TRANSMISSION_LED)
	
}
void dataIndication(const FW_DataIndication_t *params)
{

}
			









