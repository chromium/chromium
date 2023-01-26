// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fake_bluetooth_adapter.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/message_stream/fake_bluetooth_socket.h"
#include "ash/quick_pair/message_stream/fake_message_stream_lookup.h"
#include "ash/quick_pair/message_stream/message_stream.h"
#include "ash/quick_pair/message_stream/message_stream_lookup.h"
#include "ash/quick_pair/pairing/mock_pairer_broker.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector_impl.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr base::TimeDelta kRetroactiveDevicePairingTimeout = base::Seconds(60);
constexpr char kTestDeviceAddress2[] = "11:12:13:14:15:17";
constexpr char kTestBleDeviceName[] = "Test Device Name";
constexpr char kValidModelId[] = "718c17";
const std::string kUserEmail = "test@test.test";

// Expect no crash with invalid UTF-8 strings as device address.
constexpr char kTestDeviceAddress[] = "\0xff\0xff\0xff\0xff\0xff\0xff";

const std::vector<uint8_t> kModelIdBytes = {
    /*message_group=*/0x03,
    /*message_code=*/0x01,
    /*additional_data_length=*/0x00, 0x03,
    /*additional_data=*/0xAA,        0xBB, 0xCC};
const std::string kModelId = "AABBCC";

const std::vector<uint8_t> kBleAddressBytes = {
    /*message_group=*/0x03,
    /*message_code=*/0x02,
    /*additional_data_length=*/0x00,
    0x06,
    /*additional_data=*/0xAA,
    0xBB,
    0xCC,
    0xDD,
    0xEE,
    0xFF};
const std::string kBleAddress = "AA:BB:CC:DD:EE:FF";

const std::vector<uint8_t> kModelIdBleAddressBytes = {
    /*mesage_group=*/0x03,
    /*mesage_code=*/0x01,
    /*additional_data_length=*/0x00,
    0x03,
    /*additional_data=*/0xAA,
    0xBB,
    0xCC,
    /*message_group=*/0x03,
    /*message_code=*/0x02,
    /*additional_data_length=*/0x00,
    0x06,
    /*additional_data=*/0xAA,
    0xBB,
    0xCC,
    0xDD,
    0xEE,
    0xFF};

std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
CreateTestBluetoothDevice(std::string address) {
  return std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
      /*adapter=*/nullptr, /*bluetooth_class=*/0, kTestBleDeviceName, address,
      /*paired=*/true, /*connected=*/false);
}

}  // namespace

namespace ash {
namespace quick_pair {

class RetroactivePairingDetectorTest
    : public AshTestBase,
      public RetroactivePairingDetector::Observer {
 public:
  RetroactivePairingDetectorTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    pairer_broker_ = std::make_unique<MockPairerBroker>();
    mock_pairer_broker_ = static_cast<MockPairerBroker*>(pairer_broker_.get());

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
                             task_environment()->GetMainThreadTaskRunner());
    EXPECT_CALL(*mock_process_manager(), GetProcessReference)
        .WillRepeatedly([&](QuickPairProcessManager::ProcessStoppedCallback) {
          return std::make_unique<
              QuickPairProcessManagerImpl::ProcessReferenceImpl>(
              data_parser_remote_, base::DoNothing());
        });
  }

  void TearDown() override {
    retroactive_pairing_detector_.reset();
    ClearLogin();
    AshTestBase::TearDown();
  }

  void CreateRetroactivePairingDetector() {
    retroactive_pairing_detector_ =
        std::make_unique<RetroactivePairingDetectorImpl>(
            pairer_broker_.get(), message_stream_lookup_.get());
    retroactive_pairing_detector_->AddObserver(this);
  }

  MockQuickPairProcessManager* mock_process_manager() {
    return static_cast<MockQuickPairProcessManager*>(process_manager_.get());
  }

  void OnRetroactivePairFound(scoped_refptr<Device> device) override {
    retroactive_pair_found_ = true;
    retroactive_device_ = device;
  }

  void PairFastPairDeviceWithFastPair(std::string address) {
    auto fp_device = base::MakeRefCounted<Device>(kValidModelId, address,
                                                  Protocol::kFastPairInitial);
    fp_device->set_classic_address(address);
    mock_pairer_broker_->NotifyDevicePaired(fp_device);
  }

  void PairFastPairDeviceWithClassicBluetooth(bool new_paired_status,
                                              std::string classic_address) {
    bluetooth_device_ = CreateTestBluetoothDevice(classic_address);
    bluetooth_device_->AddUUID(ash::quick_pair::kFastPairBluetoothUuid);
    auto* bt_device_ptr = bluetooth_device_.get();
    adapter_->AddMockDevice(std::move(bluetooth_device_));
    adapter_->NotifyDevicePairedChanged(bt_device_ptr, new_paired_status);
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

  void Login(user_manager::UserType user_type) {
    SimulateUserLogin(kUserEmail, user_type);
  }

 protected:
  bool retroactive_pair_found_ = false;
  scoped_refptr<Device> retroactive_device_;

  scoped_refptr<FakeBluetoothAdapter> adapter_;
  std::unique_ptr<PairerBroker> pairer_broker_;
  MockPairerBroker* mock_pairer_broker_ = nullptr;

  scoped_refptr<FakeBluetoothSocket> fake_socket_ =
      base::MakeRefCounted<FakeBluetoothSocket>();
  std::unique_ptr<MessageStream> message_stream_;
  std::unique_ptr<MessageStreamLookup> message_stream_lookup_;
  FakeMessageStreamLookup* fake_message_stream_lookup_ = nullptr;
  FakeFastPairRepository fast_pair_repository_;

  mojo::SharedRemote<mojom::FastPairDataParser> data_parser_remote_;
  mojo::PendingRemote<mojom::FastPairDataParser> fast_pair_data_parser_;
  std::unique_ptr<FastPairDataParser> data_parser_;
  std::unique_ptr<QuickPairProcessManager> process_manager_;

  scoped_refptr<Device> device_;
  std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
      bluetooth_device_;

  std::unique_ptr<RetroactivePairingDetector> retroactive_pairing_detector_;
};

TEST_F(RetroactivePairingDetectorTest,
       DevicedPaired_FastPair_BluetoothEventFiresFirst) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  PairFastPairDeviceWithFastPair(kTestDeviceAddress);

  EXPECT_FALSE(retroactive_pair_found_);
}

// Regression test for b/261041950
TEST_F(RetroactivePairingDetectorTest,
       FastPairPairingEventCalledDuringBluetoothAdapterPairingEvent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBleAddressBytes);

  // Simulate the Bluetooth Adapter event firing, with the callback to
  // `IsDeviceSavedToAccount` delayed.
  fast_pair_repository_.SetIsDeviceSavedToAccountCallbackDelayed(
      /*is_delayed=*/true);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  // Simulate the Fast Pair pairing event firing during the Bluetooth Adapter
  // pairing event call stack. The Bluetooth Adapter system
  // event response has not finished completing because of the delay set in
  // `SetIsDeviceSavedToAccountCallbackDelayed`.
  PairFastPairDeviceWithFastPair(kTestDeviceAddress);

  // Trigger the callback to check the repository after the Fast Pair pairing
  // event fires. This will conclude the BluetoothAdapter pairing event call
  // stack.
  fast_pair_repository_.TriggerIsDeviceSavedToAccountCallback();

  // Simulate data being received via Message Stream for the device. It should
  // not be detected since the Fast Pair event has been fired, removing it
  // as a possible retroactive device.
  fake_socket_->TriggerReceiveCallback();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, DeviceUnpaired) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/false, kTestDeviceAddress);
  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, NoMessageStream) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, MessageStream_NoBle) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, MessageStream_NoModelId) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, MessageStream_SocketError) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  std::vector<uint8_t> data;
  SetMessageStream(data);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, MessageStream_NoBytes) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  std::vector<uint8_t> data;
  SetMessageStream(data);
  fake_socket_->SetEmptyBuffer();
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, MessageStream_Ble_ModelId_Lost) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  adapter_->RemoveMockDevice(kTestDeviceAddress);

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, MessageStream_Ble_ModelId_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});

  // Strict interpretation of opt-in status while opted in means that we
  // expect to be notified of retroactive pairing when the MessageStream
  // connects after pairing is completed. This test is for the scenario
  // when we receive both the model id and the BLE address bytes over the
  // Message Stream.
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest, MessageStream_Ble_ModelId_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  // Without the SavedDevices or StrictOptIn flags, we expect that opt-in
  // status being opted out does not matter and should not impact whether or
  // not we detect a retroactive pairing scenario. We expect to still receive
  // the model id and BLE bytes once the Message Stream connects, and be
  // notified of retroactive pairing found.
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_Ble_ModelId_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});

  // With the SavedDevices flag but without the StrictOptIn flag, we expect that
  // opt-in status being opted out does not matter and should not impact whether
  // or not we detect a retroactive pairing scenario. We expect to still receive
  // the model id and BLE bytes once the Message Stream connects, and be
  // notified of retroactive pairing found.
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_Ble_ModelId_SavedFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{features::kFastPairSavedDevices});

  // With the StrictOptIn flag but without the SavedDevices flag, we expect that
  // opt-in status being opted out does not matter and should not impact whether
  // or not we detect a retroactive pairing scenario. We expect to still receive
  // the model id and BLE bytes once the Message Stream connects, and be
  // notified of retroactive pairing found.
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_Ble_ModelId_GuestUserLoggedIn) {
  Login(user_manager::UserType::USER_TYPE_GUEST);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_Ble_ModelId_KioskUserLoggedIn) {
  Login(user_manager::UserType::USER_TYPE_KIOSK_APP);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_GetMessageStream_Ble_ModelId_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});

  // Strict interpretation of opt-in status while opted in means that we
  // expect to be notified of retroactive pairing when the Message Stream
  // connects after pairing is completed. This test is for the scenario
  // when we receive both the model id and the BLE address bytes over the
  // Message Stream. The case where we are not notified when opted out is
  // tested in Notify_OptedOut_* tests below.
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  AddMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_GetMessageStream_Ble_ModelId_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // With SavedDevices and StrictOptIn flags disabled, the user's opt-in status
  // of opted out should not impact the notification of a retroactive pairing
  // found.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  AddMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_GetMessageStream_Ble_ModelId_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // With only one flag enabled and the other disabled, the user's opt-in status
  // of opted out should not impact the notification of a retroactive pairing
  // found.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  AddMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_GetMessageStream_Ble_ModelId_SavedFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // With only one flag enabled and the other disabled, the user's opt-in status
  // of opted out should not impact the notification of a retroactive pairing
  // found.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{features::kFastPairSavedDevices});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  AddMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest,
       EnableScenarioIfLoggedInLater_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  EXPECT_FALSE(retroactive_pair_found_);

  CreateRetroactivePairingDetector();
  base::RunLoop().RunUntilIdle();

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  AddMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest,
       EnableScenarioIfLoggedInLater_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  EXPECT_FALSE(retroactive_pair_found_);

  CreateRetroactivePairingDetector();
  base::RunLoop().RunUntilIdle();

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  AddMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest,
       EnableScenarioIfLoggedInLater_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  EXPECT_FALSE(retroactive_pair_found_);

  CreateRetroactivePairingDetector();
  base::RunLoop().RunUntilIdle();

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  AddMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest,
       EnableScenarioIfLoggedInLater_SavedFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{features::kFastPairSavedDevices});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  EXPECT_FALSE(retroactive_pair_found_);

  CreateRetroactivePairingDetector();
  base::RunLoop().RunUntilIdle();

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  AddMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest,
       DontEnableScenarioIfLoggedInLaterAsGuest) {
  Login(user_manager::UserType::USER_TYPE_GUEST);
  EXPECT_FALSE(retroactive_pair_found_);

  CreateRetroactivePairingDetector();
  base::RunLoop().RunUntilIdle();

  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_GetMessageStream_Ble_ModelId_GuestUser) {
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  AddMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_GetMessageStream_ModelId_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});

  // With both SavedDevices and StrictOptIn enabled, we expect to be notified
  // when the Message Stream is connected before we are notified that the
  // pairing is complete, and retrieving the model id and BLE address
  // after the fact by parsing the previous messages received if the user
  // is opted in to saving devices to their account. The case where we are not
  // notified when opted out is tested in Notify_OptedOut_* tests below.
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  AddMessageStream(kModelIdBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_GetMessageStream_ModelId_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // With both SavedDevices and StrictOptIn disabled, we expect to be notified
  // when the Message Stream is connected before we are notified that the
  // pairing is complete, and retrieving the model id and BLE address
  // after the fact by parsing the previous messages received even if the
  // user is opted out of saving devices to their account.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  AddMessageStream(kModelIdBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_GetMessageStream_ModelId_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // With the SavedDevices flag enabled but the StrictOptIn disabled, we
  // expect to not consider the user's status of opted out and still be
  // notified when a MessageStream is connected before we are notified of
  // pairing, and successfully parse a model id and BLE address to confirm
  // retroactive pairing has been found.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  AddMessageStream(kModelIdBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_GetMessageStream_ModelId_SavedFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // With the StrictOptIn flag enabled but the SavedDevices disabled, we
  // expect to not consider the user's status of opted out and still be
  // notified when a MessageStream is connected before we are notified of
  // pairing, and successfully parse a model id and BLE address to confirm
  // retroactive pairing has been found.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{features::kFastPairSavedDevices});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  AddMessageStream(kModelIdBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_Observer_Ble_ModelId_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});

  // This test is for the scenario where the Message Stream receives messages
  // for the BLE address and model id after it is connected and paired, and
  // the detector should observe these messages and notify us of the device.
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_Observer_Ble_ModelId_GuestAccount) {
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_Observer_ModelId_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // With the SavedDevices and StrictOptIn flags enabled, we do not expect
  // to be notified if we only receive the model id (no BLE address) even if
  // the user is opted in.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_Observer_ModelId_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // With the SavedDevices and StrictOptIn flags disabled, we do not expect
  // to be notified if we only receive the model id (no BLE address) even if
  // the user is opted out. Opt-in status shouldn't matter for notifying with
  // the flags disabled, but here, since we don't have the information we need
  // from the device for retroactive pairing, then we expect to not be
  // notified.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_Observer_ModelId_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // With only the StrictOptIn flag disabled, we do not expect
  // to be notified if we only receive the model id (no BLE address) even if
  // the user is opted out. Opt-in status shouldn't matter for notifying with
  // the flags disabled, but here, since we don't have the information we need
  // from the device for retroactive pairing, then we expect to not be
  // notified.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStream_Observer_ModelId_SavedFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // With only the SavedDevices flag disabled, we do not expect
  // to be notified if we only receive the model id (no BLE address) even if
  // the user is opted out. Opt-in status shouldn't matter for notifying with
  // the flags disabled, but here, since we don't have the information we need
  // from the device for retroactive pairing, then we expect to not be
  // notified.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{features::kFastPairSavedDevices});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStreamRemovedOnDestroyed_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::test::ScopedFeatureList feature_list;

  // This test is verifying that we are notified when the MessageStream
  // is destroyed, and properly remove the MessageStream. The opt-in status
  // and flags set should not impact this behavior.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  message_stream_.reset();
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStreamRemovedOnDestroyed_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;

  // This test is verifying that we are notified when the MessageStream
  // is destroyed, and properly remove the MessageStream. The opt-in status
  // and flags set should not impact this behavior.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  message_stream_.reset();
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStreamRemovedOnDestroyed_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;

  // This test is verifying that we are notified when the MessageStream
  // is destroyed, and properly remove the MessageStream. The opt-in status
  // and flags set should not impact this behavior.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  message_stream_.reset();
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStreamRemovedOnDestroyed_SavedFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;

  // This test is verifying that we are notified when the Message Stream
  // is destroyed, and properly remove the Message Stream. The opt-in status
  // and flags set should not impact this behavior.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{features::kFastPairSavedDevices});
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  message_stream_.reset();
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStreamRemovedOnDisconnect_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // This test is verifying that we are notified when the Message Stream
  // is destroyed, and properly remove the Message Stream. The opt-in status
  // and flags set should not impact this behavior.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetErrorReason(
      device::BluetoothSocket::ErrorReason::kDisconnected);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  message_stream_ =
      std::make_unique<MessageStream>(kTestDeviceAddress, fake_socket_.get());
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->SetIOBufferFromBytes(kModelIdBytes);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStreamRemovedOnDisconnect_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // This test is verifying that we are notified when the Message Stream
  // is destroyed, and properly remove the Message Stream. The opt-in status
  // and flags set should not impact this behavior.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetErrorReason(
      device::BluetoothSocket::ErrorReason::kDisconnected);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  message_stream_ =
      std::make_unique<MessageStream>(kTestDeviceAddress, fake_socket_.get());
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->SetIOBufferFromBytes(kModelIdBytes);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStreamRemovedOnDisconnect_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // This test is verifying that we are notified when the Message Stream
  // is destroyed, and properly remove the Message Stream. The opt-in status
  // and flags set should not impact this behavior.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetErrorReason(
      device::BluetoothSocket::ErrorReason::kDisconnected);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  message_stream_ =
      std::make_unique<MessageStream>(kTestDeviceAddress, fake_socket_.get());
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->SetIOBufferFromBytes(kModelIdBytes);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       MessageStreamRemovedOnDisconnect_SavedFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // This test is verifying that we are notified when the Message Stream
  // is destroyed, and properly remove the Message Stream. The opt-in status
  // and flags set should not impact this behavior.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{features::kFastPairSavedDevices});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetErrorReason(
      device::BluetoothSocket::ErrorReason::kDisconnected);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  message_stream_ =
      std::make_unique<MessageStream>(kTestDeviceAddress, fake_socket_.get());
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->SetIOBufferFromBytes(kModelIdBytes);
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, DontNotify_OptedOut_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // If the SavedDevices and StrictOptIn flags are enabled and the user is
  // opted out, we expect not to be notified for retroactive pairing even if
  // a potential one is found.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, Notify_OptedOut_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // If the SavedDevices and StrictOptIn flags are disabled, we expect to be
  // notified when a retroactive pairing is found even if the user is opted out.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, Notify_OptedOut_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // If the SavedDevices flag is enabled but the StrictOptin flag is disabled,
  // then we expect to be notified even if the user is opted out of saving
  // devices to their account.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, Notify_OptedOut_SavedFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // If the SavedDevices flag is disabled but the StrictOptin flag is enabled,
  // then we expect to be notified even if the user is opted out of saving
  // devices to their account.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{features::kFastPairSavedDevices});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, Notify_OptedIn_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, Notify_OptedIn_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // When the strict interpretation is disabled, we expect to be notified about
  // a retroactive pairing regardless of opt-in status.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, Notify_OptedIn_SavedFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;

  // When only the SavedDevices flag is disabled, we expect to be notified about
  // a retroactive pairing regardless of opt-in status.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{features::kFastPairSavedDevices});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       DontNotify_OptedOut_OptedIn_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});

  // Simulate user is opted out.
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);

  // Simulate user is opted in. Now we would expect to be notified of a
  // retroactive pairing scenario when the flags are enabled for a
  // strict interpretation of the opt in status.
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);

  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
  EXPECT_EQ(retroactive_device_->ble_address(), kBleAddress);
  EXPECT_EQ(retroactive_device_->metadata_id(), kModelId);
}

TEST_F(RetroactivePairingDetectorTest, DontNotifyIfAlreadySavedToAcount) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  fast_pair_repository_.SaveMacAddressToAccount(kTestDeviceAddress);

  // If the SavedDevices and StrictOptIn flags are disabled, we may expect to be
  // notified when a retroactive pairing is found even if the user is opted out.
  // However, since the device is already saved the account, we expect to not
  // be notified.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

// There are two ways to get to `CheckAndRemoveIfDeviceExpired `. One is by
// `GetModelIdAndAddressFromMessageStream` which is triggered when the
// MessageStream already has the model id and BLE address messages on
// connection. The second way is through `CheckPairingInformation` which is
// triggered when the MessageStream does not have the model id and BLE
// address on connection, and the model id and BLE address are observed later
// on.
TEST_F(RetroactivePairingDetectorTest,
       DontNotify_ExpiryTimeoutReached_GetModelIdAndAddressFromMessageStream) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  // Pair a device with classic Bluetooth pairing and set the MessageStream
  // with model id and BLE address bytes to successfully detect the scenario.
  // At this point, the device is in the `device_pairing_information_` map with
  // an expiry timestamp.
  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  // Fast forward by |kRetroactiveDevicePairingTimeout| in order to simulate
  // that the device's |expiry_timestamp| has been reached. Because the
  // timeout has been reached, we expect that the retroactive pairing
  // scenario to not be triggered.
  task_environment()->FastForwardBy(kRetroactiveDevicePairingTimeout);
  fake_socket_->TriggerReceiveCallback();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       DontNotify_ExpiryTimeoutReached_CheckPairingInformation) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  // Pair a device with classic Bluetooth pairing and set the MessageStream
  // with no bytes to successfully detect the scenario.
  // At this point, the device is in the `device_pairing_information_` map with
  // an expiry timestamp. Because there are no BLE address bytes or model id
  // bytes in the connected MessageStream, the RetroactivePairingDetector adds
  // itself as an observer to wait for these messages.
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  NotifyMessageStreamConnected(kTestDeviceAddress);

  // Fast forward by |kRetroactiveDevicePairingTimeout| in order to simulate
  // that the device's |expiry_timestamp| has been reached. Because the
  // timeout has been reached, we expect that the retroactive pairing
  // scenario to not be triggered.
  task_environment()->FastForwardBy(kRetroactiveDevicePairingTimeout);

  // Set up the socket with the model id bytes and BLE address bytes to
  // successfully detect the scenario, and trigger the bytes being received
  // after the timeout to trigger the check in `CheckPairingInformation`
  // which happens in the overridden observed red functions for
  // `OnModelIdMessage` and `OnBleAddressUpdateMessage`.
  fake_socket_->SetIOBufferFromBytes(kModelIdBleAddressBytes);

  // TODO(b/263391358): Refactor `TriggerReceiveCallback` to take a
  // base::RunLoop parameter and remove `base::RunLoop().RunUntilIdle()`.
  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest,
       DontNotify_ExpiryTimeoutReached_DifferentDeviceTriggerRemoval) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  // Simulate a first device being found with classic Bluetooth pairing.
  // At this point, the device is in the `device_pairing_information_` map with
  // an expiry timestamp. This device does not have a MessageStream associated,
  // so `CheckPairingInformation` will never be fired because it doesn't have
  // a model id or BLE event, and thus alone, its expiry event will never be
  // triggered.
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  // Fast forward by |kRetroactiveDevicePairingTimeout| in order to simulate
  // that the device's |expiry_timestamp| has been reached.
  task_environment()->FastForwardBy(kRetroactiveDevicePairingTimeout);

  // Simulate another device being found for retroactive pairing. This device
  // will also be added to `device_pairing_information_` map with an expiry
  // timeout, and its addition will trigger
  // `RemoveExpiredDevicesFromStoredDeviceData`, which will remove the
  // first device since it has expired.
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress2);

  // Trigger a Message Stream event for the first device with the model id and
  // BLE address. Although there is a check in `CheckPairingInformation`,
  // the device was already removed in
  // `RemoveExpiredDevicesFromStoredDeviceData`.
  SetMessageStream(kModelIdBleAddressBytes);
  fake_socket_->TriggerReceiveCallback();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, NotifyAfterDeviceRepairs) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  CreateRetroactivePairingDetector();

  EXPECT_FALSE(retroactive_pair_found_);

  // Pair a device with classic Bluetooth pairing and set the MessageStream
  // with model id and BLE address bytes to successfully detect the scenario.
  // At this point, the device is in the `device_pairing_information_` map with
  // an expiry timestamp.
  SetMessageStream(kModelIdBleAddressBytes);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  // Simulate the device being unpaired, and paired again.
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/false, kTestDeviceAddress);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);

  // When the model id and BLE address fire, we expect the retroactive pairing
  // event to still be detected, even if the device was unpaired and repeated.
  fake_socket_->TriggerReceiveCallback();
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(retroactive_pair_found_);
}

TEST_F(RetroactivePairingDetectorTest, NoCrashWhenFootprintsResponseIsSlow) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  CreateRetroactivePairingDetector();
  EXPECT_FALSE(retroactive_pair_found_);

  // Delay the response so we can trigger it after OnDevicePaired.
  fast_pair_repository_.SetIsDeviceSavedToAccountCallbackDelayed(
      /*is_delayed=*/true);

  // The naming for these are confusing.
  // This calls DevicePairedChanged.
  PairFastPairDeviceWithClassicBluetooth(true, kTestDeviceAddress);

  // This calls OnDevicePaired.
  PairFastPairDeviceWithFastPair(kTestDeviceAddress);

  fake_socket_->TriggerReceiveCallback();
  base::RunLoop().RunUntilIdle();

  // Add a real message stream so the check passes.
  AddMessageStream(kModelIdBleAddressBytes);
  NotifyMessageStreamConnected(kTestDeviceAddress);
  base::RunLoop().RunUntilIdle();

  // Trigger the response.
  fast_pair_repository_.TriggerIsDeviceSavedToAccountCallback();
}

}  // namespace quick_pair
}  // namespace ash
