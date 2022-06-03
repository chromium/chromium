// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/message_stream/message_stream.h"

#include <memory>

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/message_stream/fake_bluetooth_socket.h"
#include "ash/services/quick_pair/fast_pair_data_parser.h"
#include "ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::vector<uint8_t> kModelIdBytes = {/*mesage_group=*/0x03,
                                            /*mesage_code=*/0x01,
                                            /*additional_data_length=*/0x00,
                                            0x03,
                                            /*additional_data=*/0xAA,
                                            0xBB,
                                            0xCC};
const std::vector<uint8_t> kInvalidBytes = {/*mesage_group=*/0x03,
                                            /*mesage_code=*/0x09,
                                            /*additional_data_length=*/0x00,
                                            0x03,
                                            /*additional_data=*/0xAA,
                                            0xBB,
                                            0xCC};
const std::vector<uint8_t> kModelIdBleAddressBytes = {
    /*mesage_group=*/0x03,
    /*mesage_code=*/0x01,
    /*additional_data_length=*/0x00,
    0x03,
    /*additional_data=*/0xAA,
    0xBB,
    0xCC,
    /*mesage_group=*/0x03,
    /*mesage_code=*/0x02,
    /*additional_data_length=*/0x00,
    0x06,
    /*additional_data=*/0xAA,
    0xBB,
    0xCC,
    0xDD,
    0xEE,
    0xFF};
const std::vector<uint8_t> kNakBytes = {/*mesage_group=*/0xFF,
                                        /*mesage_code=*/0x02,
                                        /*additional_data_length=*/0x00,
                                        0x03,
                                        /*additional_data=*/0x00,
                                        0x04,
                                        0x01};
const std::vector<uint8_t> kRingDeviceBytes = {/*mesage_group=*/0x04,
                                               /*mesage_code=*/0x01,
                                               /*additional_data_length=*/0x00,
                                               0x02,
                                               /*additional_data=*/0x01,
                                               0x3C};
const std::vector<uint8_t> kPlatformBytes = {/*mesage_group=*/0x03,
                                             /*mesage_code=*/0x08,
                                             /*additional_data_length=*/0x00,
                                             0x02,
                                             /*additional_data=*/0x01,
                                             0x1C};
const std::vector<uint8_t> kActiveComponentBytes = {
    /*mesage_group=*/0x03, /*mesage_code=*/0x06,
    /*additional_data_length=*/0x00, 0x01,
    /*additional_data=*/0x03};
const std::vector<uint8_t> kRemainingBatteryTimeBytes = {
    /*mesage_group=*/0x03,           /*mesage_code=*/0x04,
    /*additional_data_length=*/0x00, 0x02,
    /*additional_data=*/0x01,        0x0F};
const std::vector<uint8_t> kBatteryUpdateBytes = {
    /*mesage_group=*/0x03,
    /*mesage_code=*/0x03,
    /*additional_data_length=*/0x00,
    0x03,
    /*additional_data=*/0x57,
    0x41,
    0x7F};
const std::string kModelIdString = "AABBCC";
const std::string kBleAddressString = "AA:BB:CC:DD:EE:FF";

constexpr int kMaxRetryCount = 10;
constexpr int kMessageStorageCapacity = 1000;
constexpr char kTestDeviceAddress[] = "11:12:13:14:15:16";

}  // namespace

namespace ash {
namespace quick_pair {

class MessageStreamTest : public testing::Test, public MessageStream::Observer {
 public:
  void SetUp() override {
    process_manager_ = std::make_unique<MockQuickPairProcessManager>();
    quick_pair_process::SetProcessManager(process_manager_.get());

    data_parser_ = std::make_unique<FastPairDataParser>(
        fast_pair_data_parser_.InitWithNewPipeAndPassReceiver());

    data_parser_remote_.Bind(std::move(fast_pair_data_parser_),
                             task_enviornment_.GetMainThreadTaskRunner());

    EXPECT_CALL(*mock_process_manager(), GetProcessReference)
        .WillRepeatedly([&](QuickPairProcessManager::ProcessStoppedCallback) {
          return std::make_unique<
              QuickPairProcessManagerImpl::ProcessReferenceImpl>(
              data_parser_remote_, base::DoNothing());
        });

    message_stream_ =
        std::make_unique<MessageStream>(kTestDeviceAddress, fake_socket_.get());
  }

  void AddObserver() { message_stream_->AddObserver(this); }

  MockQuickPairProcessManager* mock_process_manager() {
    return static_cast<MockQuickPairProcessManager*>(process_manager_.get());
  }

  void SetSuccessMessageStreamMessage(const std::vector<uint8_t>& bytes) {
    fake_socket_->SetIOBufferFromBytes(bytes);
  }

  void TriggerReceiveSuccessCallback() {
    fake_socket_->TriggerReceiveCallback();
  }

  void OnModelIdMessage(const std::string& device_address,
                        const std::string& model_id) override {
    model_id_ = std::move(model_id);
  }

  void OnMessageStreamDestroyed(const std::string& device_address) override {
    on_destroyed_ = true;
  }

  void CleanUpMemory() { message_stream_.reset(); }

  void DisconnectSocket() {
    fake_socket_->SetErrorReason(
        device::BluetoothSocket::ErrorReason::kDisconnected);
  }

  void SetEmptyBuffer() { fake_socket_->SetEmptyBuffer(); }

  void OnDisconnected(const std::string& device_address) override {
    on_socket_disconnected_ = true;
  }

  void OnBleAddressUpdateMessage(const std::string& device_address,
                                 const std::string& ble_address) override {
    ble_address_ = std::move(ble_address);
  }

  void OnBatteryUpdateMessage(
      const std::string& device_address,
      const mojom::BatteryUpdatePtr& battery_update) override {
    battery_update_ = true;
  }

  void OnRemainingBatteryTimeMessage(const std::string& device_address,
                                     uint16_t remaining_battery_time) override {
    remaining_battery_time_ = remaining_battery_time;
  }

  void OnEnableSilenceModeMessage(const std::string& device_address,
                                  bool enable_silence_mode) override {
    enable_silence_mode_ = enable_silence_mode;
  }

  void OnCompanionAppLogBufferFullMessage(
      const std::string& device_address) override {
    log_buffer_full_ = true;
  }

  void OnActiveComponentsMessage(const std::string& device_address,
                                 uint8_t active_components_byte) override {
    active_components_byte_ = active_components_byte;
  }

  void OnRingDeviceMessage(const std::string& device_address,
                           const mojom::RingDevicePtr& ring_device) override {
    ring_device_ = true;
  }

  void OnAcknowledgementMessage(
      const std::string& device_address,
      const mojom::AcknowledgementMessagePtr& acknowledgement) override {
    acknowledgement_ = true;
  }

  void OnAndroidSdkVersionMessage(const std::string& device_address,
                                  uint8_t sdk_version) override {
    sdk_version_ = sdk_version;
  }

 protected:
  scoped_refptr<FakeBluetoothSocket> fake_socket_ =
      base::MakeRefCounted<FakeBluetoothSocket>();
  std::unique_ptr<MessageStream> message_stream_;
  std::string model_id_;
  std::string ble_address_;
  base::test::SingleThreadTaskEnvironment task_enviornment_;
  std::unique_ptr<QuickPairProcessManager> process_manager_;
  mojo::SharedRemote<mojom::FastPairDataParser> data_parser_remote_;
  mojo::PendingRemote<mojom::FastPairDataParser> fast_pair_data_parser_;
  std::unique_ptr<FastPairDataParser> data_parser_;
  bool battery_update_ = false;
  uint16_t remaining_battery_time_ = 0;
  bool enable_silence_mode_ = false;
  bool on_destroyed_ = false;
  bool on_socket_disconnected_ = false;
  bool log_buffer_full_ = false;
  uint8_t active_components_byte_ = 0x0F;
  bool ring_device_ = false;
  bool acknowledgement_ = false;
  uint8_t sdk_version_ = 0;
};

TEST_F(MessageStreamTest, ReceiveMessages_Observation_SuccessfulMessage) {
  EXPECT_TRUE(model_id_.empty());
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kModelIdBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(model_id_, kModelIdString);
  EXPECT_FALSE(message_stream_->messages().empty());
}

TEST_F(MessageStreamTest, ReceiveMessages_Observation_NullMessage) {
  EXPECT_TRUE(model_id_.empty());
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kInvalidBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(message_stream_->messages().empty());
  EXPECT_TRUE(model_id_.empty());
}

TEST_F(MessageStreamTest, ReceiveMessages_GetMessages) {
  EXPECT_TRUE(model_id_.empty());
  EXPECT_TRUE(message_stream_->messages().empty());

  SetSuccessMessageStreamMessage(kModelIdBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
  EXPECT_EQ(message_stream_->messages()[0]->get_model_id(), kModelIdString);
}

TEST_F(MessageStreamTest, ReceiveMessages_CleanUp) {
  EXPECT_TRUE(model_id_.empty());
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kModelIdBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
  EXPECT_FALSE(on_destroyed_);
  CleanUpMemory();
  EXPECT_TRUE(on_destroyed_);
}

TEST_F(MessageStreamTest, ReceiveMessages_SocketDisconnect) {
  EXPECT_FALSE(on_socket_disconnected_);
  EXPECT_TRUE(message_stream_->messages().empty());

  DisconnectSocket();

  AddObserver();
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(message_stream_->messages().empty());
  EXPECT_TRUE(on_socket_disconnected_);
}

TEST_F(MessageStreamTest, ReceiveMessages_FailureAfterMaxRetries) {
  EXPECT_FALSE(on_socket_disconnected_);
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();

  for (int i = 0; i < kMaxRetryCount; ++i) {
    TriggerReceiveSuccessCallback();
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_TRUE(message_stream_->messages().empty());
  EXPECT_TRUE(on_socket_disconnected_);
}

TEST_F(MessageStreamTest,
       ReceiveMessages_Observation_SuccessfulMultipleMessages) {
  EXPECT_TRUE(model_id_.empty());
  EXPECT_TRUE(ble_address_.empty());
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kModelIdBleAddressBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
  EXPECT_EQ(model_id_, kModelIdString);
  EXPECT_EQ(ble_address_, kBleAddressString);
}

TEST_F(MessageStreamTest, ReceiveMessages_GetMultipleMessages) {
  EXPECT_TRUE(model_id_.empty());
  EXPECT_TRUE(ble_address_.empty());
  EXPECT_TRUE(message_stream_->messages().empty());

  SetSuccessMessageStreamMessage(kModelIdBleAddressBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
  EXPECT_EQ(message_stream_->messages()[0]->get_model_id(), kModelIdString);
  EXPECT_EQ(message_stream_->messages()[1]->get_ble_address_update(),
            kBleAddressString);
}

TEST_F(MessageStreamTest, ReceiveMessages_BatteryUpdate) {
  EXPECT_FALSE(battery_update_);
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kBatteryUpdateBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
  EXPECT_TRUE(battery_update_);
}

TEST_F(MessageStreamTest, ReceiveMessages_RemainingBatteryTime) {
  EXPECT_EQ(remaining_battery_time_, 0);
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kRemainingBatteryTimeBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
  EXPECT_EQ(remaining_battery_time_, 271);
}

TEST_F(MessageStreamTest, ReceiveMessages_ActiveComponents) {
  EXPECT_EQ(active_components_byte_, 0x0F);
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kActiveComponentBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
  EXPECT_EQ(active_components_byte_, 0x03);
}

TEST_F(MessageStreamTest, ReceiveMessages_SdkVersion) {
  EXPECT_EQ(sdk_version_, 0);
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kPlatformBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
  EXPECT_EQ(sdk_version_, 28);
}

TEST_F(MessageStreamTest, ReceiveMessages_RingDevice) {
  EXPECT_FALSE(ring_device_);
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kRingDeviceBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
  EXPECT_TRUE(ring_device_);
}

TEST_F(MessageStreamTest, ReceiveMessages_Nak) {
  EXPECT_FALSE(acknowledgement_);
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kNakBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
  EXPECT_TRUE(acknowledgement_);
}

TEST_F(MessageStreamTest, ReceiveMessages_EmptyBuffer_SuccessReceive) {
  EXPECT_TRUE(message_stream_->messages().empty());

  SetEmptyBuffer();
  SetSuccessMessageStreamMessage(kModelIdBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(message_stream_->messages().empty());
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
}

TEST_F(MessageStreamTest, ReceiveMessages_EmptyBuffer_ErrorReceive) {
  EXPECT_TRUE(message_stream_->messages().empty());

  SetEmptyBuffer();
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(message_stream_->messages().empty());
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(message_stream_->messages().empty());
}

TEST_F(MessageStreamTest, ReceiveMessages_BufferFull) {
  EXPECT_TRUE(model_id_.empty());
  EXPECT_TRUE(ble_address_.empty());

  AddObserver();

  for (int i = 0; i < kMessageStorageCapacity + 1; i++) {
    SetSuccessMessageStreamMessage(kModelIdBytes);
    TriggerReceiveSuccessCallback();
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_EQ(static_cast<int>(message_stream_->messages().size()),
            kMessageStorageCapacity);
}

}  // namespace quick_pair
}  // namespace ash
