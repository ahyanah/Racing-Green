#include "variant.h"
#include <due_can.h>

#define CAN0_TX_PRIO  15

#define NDRIVE_RXID  0x210
#define NDRIVE_TXID  0x180
#define NDRIVE_BPS   CAN_BPS_250K

#define REG_N_ACTUAL 0x30

#define DEBUG

void print_can_frame(RX_CAN_FRAME frame) {
  Serial.print("ID:");
  Serial.print(frame.id, HEX);
  Serial.print("\t");
  Serial.print("FID:");
  Serial.print(frame.fid, HEX);
  Serial.print("\t");
  Serial.print("RTR:");
  Serial.print(frame.rtr, HEX);
  Serial.print("\t");
  Serial.print("IDE:");
  Serial.print(frame.ide, HEX);
  Serial.print("\t");
  Serial.print("DLC:");
  Serial.print(frame.dlc, HEX);
  Serial.print("\t");
  Serial.print("Data:");
  Serial.print("\t");
  for (int i = 0; i < frame.dlc; i++) {
    Serial.print(frame.data[i], HEX);
    Serial.print("\t");
  }
  Serial.println();
}

void bamocar_request_transfer(uint8_t regid, uint8_t time) {
  TX_CAN_FRAME txFrame;
  txFrame.id = NDRIVE_RXID;
  txFrame.dlc = 3;
  txFrame.data[0] = 0x3d;
  txFrame.data[1] = regid;
  txFrame.data[2] = time;
  CAN.sendFrame(txFrame);
}

void bamocar_abort_transfer(uint8_t regid) {
  TX_CAN_FRAME txFrame;
  txFrame.id = NDRIVE_RXID;
  txFrame.dlc = 3;
  txFrame.data[0] = 0x3d;
  txFrame.data[1] = regid;
  txFrame.data[2] = 0xff;
  CAN.sendFrame(txFrame);
}

inline void setup_serial() {
  // start serial port at 115200 bps: 
  Serial.begin(115200);
}

inline void setup_can_hardware() {
  // Verify CAN0 and CAN1 initialization
  if (
    CAN.init(SystemCoreClock, NDRIVE_BPS) &&
    CAN2.init(SystemCoreClock, NDRIVE_BPS)
  ) {

    // Disable all CAN0 & CAN1 interrupts
    CAN.disable_interrupt(CAN_DISABLE_ALL_INTERRUPT_MASK);
    CAN2.disable_interrupt(CAN_DISABLE_ALL_INTERRUPT_MASK);

    NVIC_EnableIRQ(CAN0_IRQn);
    NVIC_EnableIRQ(CAN1_IRQn);
    
    #ifdef DEBUG
    Serial.println("CAN initialization OK");
    #endif
  }
  else {
    #ifdef DEBUG
    Serial.println("CAN initialization (sync) ERROR");
    #endif
  }
}

inline void setup_can_mailbox() {
  CAN.reset_all_mailbox();

  CAN.mailbox_set_mode(0, CAN_MB_RX_MODE);
  CAN.mailbox_set_accept_mask(0, 0x1FFFFFFF, false);
  CAN.mailbox_set_id(0, NDRIVE_TXID, false);

  CAN.mailbox_set_mode(1, CAN_MB_TX_MODE);
  CAN.mailbox_set_priority(1, CAN0_TX_PRIO);
  CAN.mailbox_set_accept_mask(1, 0x1FFFFFFF, false);

  CAN.enable_interrupt(CAN_IER_MB0);
}

inline void setup_can_sniffer_for_debugging() {
  CAN2.reset_all_mailbox();

  CAN2.mailbox_set_mode(0, CAN_MB_RX_MODE);
  CAN2.mailbox_set_accept_mask(0, 0x1FFFFFFF, false);
  CAN2.mailbox_set_id(0, NDRIVE_RXID, false);

  CAN2.enable_interrupt(CAN_IER_MB0);
}

void setup() {
  setup_serial();
  setup_can_hardware();
  setup_can_mailbox();
  
  #ifdef DEBUG
  setup_can_sniffer_for_debugging();
  #endif

  test_1();
}

// Test 1: request "N Actual"
static void test_1(void)
{
  //CAN.global_send_transfer_cmd(CAN_TCR_MB1);
  bamocar_abort_transfer(REG_N_ACTUAL);
  delayMicroseconds(1000);
  bamocar_request_transfer(REG_N_ACTUAL, 100);
  delayMicroseconds(1000);
}

// Response parser
void parse_response(RX_CAN_FRAME rxFrame) {
  if (rxFrame.data[0] == REG_N_ACTUAL) {
    float value = (rxFrame.data[1] | (rxFrame.data[2] << 8)) / 32767.0 * 100;
    Serial.print(rxFrame.data[1] | (rxFrame.data[2] << 8), HEX);
    Serial.print("\t");
    Serial.println(value);
  }
}

void loop() {
  RX_CAN_FRAME inFrame;

  if (CAN.rx_avail()) {
    CAN.get_rx_buff(&inFrame);

    #ifdef DEBUG
    Serial.print("CAN\t");
    print_can_frame(inFrame);
    #endif

    #ifndef DEBUG
    parse_response(inFrame);
    #endif

    delayMicroseconds(100);
  }

  if (CAN2.rx_avail()) { 
    CAN2.get_rx_buff(&inFrame);

    #ifdef DEBUG
    Serial.print("CAN2\t");
    print_can_frame(inFrame);
    #endif

    delayMicroseconds(100);
  }
}
