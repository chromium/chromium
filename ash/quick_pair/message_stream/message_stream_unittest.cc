// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/message_stream/message_stream.h"

#include <memory>

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/message_stream/fake_bluetooth_socket.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/quick_pair/fast_pair_data_parser.h"
#include "chromeos/ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"
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
const std::vector<uint8_t> kEnableSilenceModeBytes = {
    /*mesage_group=*/0x01,
    /*mesage_code=*/0x01,
    /*additional_data_length=*/0x00, 0x00};
const std::vector<uint8_t> kCompanionAppLogBufferFullBytes = {
    /*mesage_group=*/0x02,
    /*mesage_code=*/0x01,
    /*additional_data_length=*/0x00, 0x00};
const std::string kModelIdString = "AABBCC";
const std::string kBleAddressString = "AA:BB:CC:DD:EE:FF";

constexpr int kMaxRetryCount = 10;
constexpr int kMessageStorageCapacity = 1000;
constexpr char kTestDeviceAddress[] = "11:12:13:14:15:16";

const char kMessageStreamReceiveResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.Receive.Result";
const char kMessageStreamReceiveErrorMetric[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.Receive.ErrorReason";

class FakeQuickPairProcessManager
    : public ash::quick_pair::QuickPairProcessManager {
 public:
  FakeQuickPairProcessManager(
      base::test::SingleThreadTaskEnvironment* task_environment)
      : task_enviornment_(task_environment) {
    data_parser_ = std::make_unique<ash::quick_pair::FastPairDataParser>(
        fast_pair_data_parser_.InitWithNewPipeAndPassReceiver());

    data_parser_remote_.Bind(std::move(fast_pair_data_parser_),
                             task_enviornment_->GetMainThreadTaskRunner());
  }

  ~FakeQuickPairProcessManager() override = default;

  std::unique_ptr<ProcessReference> GetProcessReference(
      ProcessStoppedCallback on_process_stopped_callback) override {
    on_process_stopped_callback_ = std::move(on_process_stopped_callback);

    if (process_stopped_) {
      std::move(on_process_stopped_callback_)
          .Run(ash::quick_pair::QuickPairProcessManager::ShutdownReason::
                   kFastPairDataParserMojoPipeDisconnection);
    }

    return std::make_unique<
        ash::quick_pair::QuickPairProcessManagerImpl::ProcessReferenceImpl>(
        data_parser_remote_, base::DoNothing());
  }

  void SetProcessStopped(bool process_stopped) {
    process_stopped_ = process_stopped;
  }

 private:
  bool process_stopped_ = false;
  mojo::SharedRemote<ash::quick_pair::mojom::FastPairDataParser>
      data_parser_remote_;
  mojo::PendingRemote<ash::quick_pair::mojom::FastPairDataParser>
      fast_pair_data_parser_;
  std::unique_ptr<ash::quick_pair::FastPairDataParser> data_parser_;
  raw_ptr<base::test::SingleThreadTaskEnvironment> task_enviornment_;
  ProcessStoppedCallback on_process_stopped_callback_;
};

}  // namespace

namespace ash {
namespace quick_pair {

class MessageStreamTest : public testing::Test, public MessageStream::Observer {
 public:
  void SetUp() override {
    process_manager_ =
        std::make_unique<FakeQuickPairProcessManager>(&task_enviornment_);
    quick_pair_process::SetProcessManager(process_manager_.get());
    fake_process_manager_ =
        static_cast<FakeQuickPairProcessManager*>(process_manager_.get());

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

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  base::HistogramTester histogram_tester_;
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
  raw_ptr<FakeQuickPairProcessManager> fake_process_manager_;
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
  histogram_tester().ExpectTotalCount(kMessageStreamReceiveResultMetric, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamReceiveErrorMetric, 0);

  EXPECT_TRUE(model_id_.empty());
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kModelIdBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(model_id_, kModelIdString);
  EXPECT_FALSE(message_stream_->messages().empty());

  histogram_tester().ExpectTotalCount(kMessageStreamReceiveResultMetric, 1);
  histogram_tester().ExpectTotalCount(kMessageStreamReceiveErrorMetric, 0);
}

TEST_F(MessageStreamTest, ReceiveMessages_Observation_NullMessage) {
  histogram_tester().ExpectTotalCount(kMessageStreamReceiveResultMetric, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamReceiveErrorMetric, 0);

  EXPECT_TRUE(model_id_.empty());
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kInvalidBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(message_stream_->messages().empty());
  EXPECT_TRUE(model_id_.empty());

  histogram_tester().ExpectTotalCount(kMessageStreamReceiveResultMetric, 1);
  histogram_tester().ExpectTotalCount(kMessageStreamReceiveErrorMetric, 0);
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
  histogram_tester().ExpectTotalCount(kMessageStreamReceiveResultMetric, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamReceiveErrorMetric, 0);

  EXPECT_FALSE(on_socket_disconnected_);
  EXPECT_TRUE(message_stream_->messages().empty());

  DisconnectSocket();

  AddObserver();
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(message_stream_->messages().empty());
  EXPECT_TRUE(on_socket_disconnected_);

  histogram_tester().ExpectTotalCount(kMessageStreamReceiveResultMetric, 1);
  histogram_tester().ExpectTotalCount(kMessageStreamReceiveErrorMetric, 1);
}

TEST_F(MessageStreamTest,
       ReceiveMessages_DisconnectCallback_SocketAlreadyDisconnected) {
  EXPECT_FALSE(on_socket_disconnected_);
  EXPECT_TRUE(message_stream_->messages().empty());
  DisconnectSocket();
  AddObserver();
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(message_stream_->messages().empty());
  EXPECT_TRUE(on_socket_disconnected_);

  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run).Times(1);
  message_stream_->Disconnect(callback.Get());
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

TEST_F(MessageStreamTest, ReceiveMessages_EnableSilenceMode) {
  EXPECT_FALSE(enable_silence_mode_);
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kEnableSilenceModeBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
  EXPECT_TRUE(enable_silence_mode_);
}

TEST_F(MessageStreamTest, ReceiveMessages_CompanionAppLogBuffer) {
  EXPECT_FALSE(log_buffer_full_);
  EXPECT_TRUE(message_stream_->messages().empty());

  AddObserver();
  SetSuccessMessageStreamMessage(kCompanionAppLogBufferFullBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
  EXPECT_TRUE(log_buffer_full_);
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

TEST_F(MessageStreamTest, UtilityProcessStopped_RetrySuccess) {
  EXPECT_TRUE(message_stream_->messages().empty());
  fake_process_manager_->SetProcessStopped(true);
  SetSuccessMessageStreamMessage(kModelIdBytes);
  TriggerReceiveSuccessCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(message_stream_->messages().empty());
}

}  // namespace quick_pair
}  // namespace ash
