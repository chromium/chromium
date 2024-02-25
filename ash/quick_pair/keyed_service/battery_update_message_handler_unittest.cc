// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/battery_update_message_handler.h"

#include <memory>
#include <optional>

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fake_bluetooth_adapter.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/message_stream/fake_bluetooth_socket.h"
#include "ash/quick_pair/message_stream/fake_message_stream_lookup.h"
#include "ash/quick_pair/message_stream/message_stream.h"
#include "ash/quick_pair/message_stream/message_stream_lookup.h"
#include "ash/quick_pair/pairing/mock_pairer_broker.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/quick_pair/fast_pair_data_parser.h"
#include "chromeos/ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestDeviceAddress[] = "11:12:13:14:15:16";
constexpr char kTestBleDeviceName[] = "Test Device Name";

std::vector<uint8_t> kBatteryUpdateBytes1 = {/*mesage_group=*/0x03,
                                             /*mesage_code=*/0x03,
                                             /*additional_data_length=*/0x00,
                                             0x03,
                                             /*additional_data=*/0x57,
                                             0x41,
                                             0x7F};
std::vector<uint8_t> kBatteryUpdateBytes2 = {/*mesage_group=*/0x03,
                                             /*mesage_code=*/0x03,
                                             /*additional_data_length=*/0x00,
                                             0x03,
                                             /*additional_data=*/0x51,
                                             0x38,
                                             0x38};

const std::vector<uint8_t> kModelIdBytes = {
    /*message_group=*/0x03,
    /*message_code=*/0x01,
    /*additional_data_length=*/0x00, 0x03,
    /*additional_data=*/0xAA,        0xBB, 0xCC};

std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
CreateTestBluetoothDevice(std::string address,
                          device::MockBluetoothAdapter* adapter) {
  return std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
      /*adapter=*/adapter, /*bluetooth_class=*/0, kTestBleDeviceName, address,
      /*paired=*/true, /*connected=*/false);
}

}  // namespace

namespace ash {
namespace quick_pair {

class BatteryUpdateMessageHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
        bluetooth_device =
            CreateTestBluetoothDevice(kTestDeviceAddress, adapter_.get());
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    bluetooth_device_ = bluetooth_device.get();
    adapter_->AddMockDevice(std::move(bluetooth_device));

    message_stream_lookup_ = std::make_unique<FakeMessageStreamLookup>();
    fake_message_stream_lookup_ =
        static_cast<FakeMessageStreamLookup*>(message_stream_lookup_.get());
    message_stream_ =
        std::make_unique<MessageStream>(kTestDeviceAddress, fake_socket_.get());

    process_manager_ = std::make_unique<MockQuickPairProcessManager>();
    quick_pair_process::SetProcessManager(process_manager_.get());
    data_parser_ = std::make_unique<FastPairDataParser>(
        fast_pair_data_parser_.InitWithNewPipeAndPassReceiver());
    data_parser_remote_.Bind(std::move(fast_pair_data_parser_),
                             task_environment_.GetMainThreadTaskRunner());
    EXPECT_CALL(*mock_process_manager(), GetProcessReference)
        .WillRepeatedly([&](QuickPairProcessManager::ProcessStoppedCallback) {
          return std::make_unique<
              QuickPairProcessManagerImpl::ProcessReferenceImpl>(
              data_parser_remote_, base::DoNothing());
        });

    battery_update_message_handler_ =
        std::make_unique<BatteryUpdateMessageHandler>(
            message_stream_lookup_.get());
  }

  void TearDown() override {
    fake_message_stream_lookup_->RemoveMessageStream(kTestDeviceAddress);
    battery_update_message_handler_.reset();
  }

  MockQuickPairProcessManager* mock_process_manager() {
    return static_cast<MockQuickPairProcessManager*>(process_manager_.get());
  }

  void SetMessageStream(const std::vector<uint8_t>& message_bytes) {
    fake_socket_->SetIOBufferFromBytes(message_bytes);
    message_stream_ =
        std::make_unique<MessageStream>(kTestDeviceAddress, fake_socket_.get());
  }

  void AddMessageStream(const std::vector<uint8_t>& message_bytes) {
    fake_socket_->SetIOBufferFromBytes(message_bytes);
    message_stream_ =
        std::make_unique<MessageStream>(kTestDeviceAddress, fake_socket_.get());
    fake_message_stream_lookup_->AddMessageStream(kTestDeviceAddress,
                                                  message_stream_.get());
  }

  void NotifyMessageStreamConnected(std::string device_address) {
    fake_message_stream_lookup_->NotifyMessageStreamConnected(
        device_address, message_stream_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<FakeBluetoothAdapter> adapter_;

  scoped_refptr<FakeBluetoothSocket> fake_socket_ =
      base::MakeRefCounted<FakeBluetoothSocket>();
  std::unique_ptr<MessageStream> message_stream_;
  std::unique_ptr<MessageStreamLookup> message_stream_lookup_;
  raw_ptr<FakeMessageStreamLookup> fake_message_stream_lookup_ = nullptr;

  mojo::SharedRemote<mojom::FastPairDataParser> data_parser_remote_;
  mojo::PendingRemote<mojom::FastPairDataParser> fast_pair_data_parser_;
  std::unique_ptr<FastPairDataParser> data_parser_;
  std::unique_ptr<QuickPairProcessManager> process_manager_;

  raw_ptr<device::BluetoothDevice, DanglingUntriaged> bluetooth_device_ =
      nullptr;
  std::unique_ptr<BatteryUpdateMessageHandler> battery_update_message_handler_;
};

TEST_F(BatteryUpdateMessageHandlerTest, BatteryUpdate_GetMessages) {
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));

  SetMessageStream(kBatteryUpdateBytes1);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_NE(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_NE(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));
}

TEST_F(BatteryUpdateMessageHandlerTest, BatteryUpdate_Observation) {
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));

  fake_socket_->SetIOBufferFromBytes(kBatteryUpdateBytes1);
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_NE(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_NE(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));
}

TEST_F(BatteryUpdateMessageHandlerTest, BatteryUpdate_MultipleMessages) {
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));

  SetMessageStream(kBatteryUpdateBytes1);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      bluetooth_device_
          ->GetBatteryInfo(
              device::BluetoothDevice::BatteryType::kLeftBudTrueWireless)
          ->percentage);
  EXPECT_EQ(87,
            bluetooth_device_
                ->GetBatteryInfo(
                    device::BluetoothDevice::BatteryType::kLeftBudTrueWireless)
                ->percentage.value());
  EXPECT_TRUE(
      bluetooth_device_
          ->GetBatteryInfo(
              device::BluetoothDevice::BatteryType::kRightBudTrueWireless)
          ->percentage);
  EXPECT_EQ(65,
            bluetooth_device_
                ->GetBatteryInfo(
                    device::BluetoothDevice::BatteryType::kRightBudTrueWireless)
                ->percentage.value());

  EXPECT_FALSE(bluetooth_device_
                   ->GetBatteryInfo(
                       device::BluetoothDevice::BatteryType::kCaseTrueWireless)
                   ->percentage);

  fake_socket_->SetIOBufferFromBytes(kBatteryUpdateBytes2);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      bluetooth_device_
          ->GetBatteryInfo(
              device::BluetoothDevice::BatteryType::kLeftBudTrueWireless)
          ->percentage);
  EXPECT_EQ(81,
            bluetooth_device_
                ->GetBatteryInfo(
                    device::BluetoothDevice::BatteryType::kLeftBudTrueWireless)
                ->percentage.value());
  EXPECT_TRUE(
      bluetooth_device_
          ->GetBatteryInfo(
              device::BluetoothDevice::BatteryType::kRightBudTrueWireless)
          ->percentage);
  EXPECT_EQ(56,
            bluetooth_device_
                ->GetBatteryInfo(
                    device::BluetoothDevice::BatteryType::kRightBudTrueWireless)
                ->percentage.value());

  EXPECT_TRUE(bluetooth_device_
                  ->GetBatteryInfo(
                      device::BluetoothDevice::BatteryType::kCaseTrueWireless)
                  ->percentage);
  EXPECT_EQ(56, bluetooth_device_
                    ->GetBatteryInfo(
                        device::BluetoothDevice::BatteryType::kCaseTrueWireless)
                    ->percentage.value());
}

TEST_F(BatteryUpdateMessageHandlerTest, NoBatteryUpdate_GetMessages) {
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));

  SetMessageStream(kModelIdBytes);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));
}

TEST_F(BatteryUpdateMessageHandlerTest, NoBatteryUpdate_Observation) {
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));

  fake_socket_->SetIOBufferFromBytes(kModelIdBytes);
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));
}

TEST_F(BatteryUpdateMessageHandlerTest, MessageStreamRemovedOnDestroyed) {
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));

  SetMessageStream(kBatteryUpdateBytes1);
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  message_stream_.reset();
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));
}

TEST_F(BatteryUpdateMessageHandlerTest, MessageStreamRemovedOnDisconnect) {
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));

  fake_socket_->SetErrorReason(
      device::BluetoothSocket::ErrorReason::kDisconnected);
  message_stream_ =
      std::make_unique<MessageStream>(kTestDeviceAddress, fake_socket_.get());
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));
}

TEST_F(BatteryUpdateMessageHandlerTest,
       MessageStreamRemovedOnDisconnect_MessageStreamDestroted) {
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));

  fake_socket_->SetErrorReason(
      device::BluetoothSocket::ErrorReason::kDisconnected);
  message_stream_ =
      std::make_unique<MessageStream>(kTestDeviceAddress, fake_socket_.get());
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));

  SetMessageStream(kBatteryUpdateBytes1);
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  message_stream_.reset();
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));
}

TEST_F(BatteryUpdateMessageHandlerTest, DeviceLost) {
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device_->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));

  SetMessageStream(kBatteryUpdateBytes1);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  auto bluetooth_device = adapter_->RemoveMockDevice(kTestDeviceAddress);

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::nullopt,
            bluetooth_device->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_EQ(std::nullopt,
            bluetooth_device->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));
}

}  // namespace quick_pair
}  // namespace ash
