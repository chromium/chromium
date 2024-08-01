// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner_impl.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/quick_pair/scanning/fast_pair/fake_fast_pair_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/quick_pair/fast_pair_data_parser.h"
#include "chromeos/ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kValidModelId[] = "718c17";
const std::string kAddress = "test_address";

class FakeQuickPairProcessManager
    : public ash::quick_pair::QuickPairProcessManager {
 public:
  FakeQuickPairProcessManager(
      base::test::SingleThreadTaskEnvironment* task_environment)
      : task_environment_(task_environment) {
    data_parser_ = std::make_unique<ash::quick_pair::FastPairDataParser>(
        fast_pair_data_parser_.InitWithNewPipeAndPassReceiver());

    data_parser_remote_.Bind(std::move(fast_pair_data_parser_),
                             task_environment_->GetMainThreadTaskRunner());
  }

  ~FakeQuickPairProcessManager() override = default;

  std::unique_ptr<ProcessReference> GetProcessReference(
      ProcessStoppedCallback on_process_stopped_callback) override {
    on_process_stopped_callback_ = std::move(on_process_stopped_callback);

    if (process_stopped_) {
      std::move(on_process_stopped_callback_)
          .Run(
              ash::quick_pair::QuickPairProcessManager::ShutdownReason::kCrash);
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
  raw_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
  ProcessStoppedCallback on_process_stopped_callback_;
};

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairDiscoverableScannerImplTest : public testing::Test {
 public:
  void SetUp() override {
    NetworkHandler::Initialize();
    repository_ = std::make_unique<FakeFastPairRepository>();

    nearby::fastpair::Device metadata;
    metadata.set_trigger_distance(2);
    metadata.set_device_type(
        nearby::fastpair::DeviceType::TRUE_WIRELESS_HEADPHONES);

    nearby::fastpair::Status* status = metadata.mutable_status();
    status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);

    repository_->SetFakeMetadata(kValidModelId, metadata);

    scanner_ = base::MakeRefCounted<FakeFastPairScanner>();

    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    process_manager_ =
        std::make_unique<FakeQuickPairProcessManager>(&task_environment_);
    quick_pair_process::SetProcessManager(process_manager_.get());
    fake_process_manager_ =
        static_cast<FakeQuickPairProcessManager*>(process_manager_.get());

    discoverable_scanner_ = std::make_unique<FastPairDiscoverableScannerImpl>(
        scanner_, adapter_, found_device_callback_.Get(),
        lost_device_callback_.Get());
  }

  void TearDown() override {
    process_manager_.reset();
    testing::Test::TearDown();
    discoverable_scanner_.reset();
    NetworkHandler::Shutdown();
  }

  MockQuickPairProcessManager* mock_process_manager() {
    return static_cast<MockQuickPairProcessManager*>(process_manager_.get());
  }

 protected:
  device::BluetoothDevice* GetDevice(const std::string& hex_model_id,
                                     bool is_paired = false) {
    auto device = std::make_unique<device::MockBluetoothDevice>(
        adapter_.get(), 0, "test_name", kAddress, /*paired=*/is_paired,
        /*connected=*/false);

    if (!hex_model_id.empty()) {
      std::vector<uint8_t> model_id_bytes;
      base::HexStringToBytes(hex_model_id, &model_id_bytes);
      device->SetServiceDataForUUID(kFastPairBluetoothUuid, model_id_bytes);
    }

    device::BluetoothDevice* device_ptr = device.get();

    adapter_->AddMockDevice(std::move(device));
    ON_CALL(*adapter_, GetDevice(kAddress))
        .WillByDefault(testing::Return(device_ptr));

    return device_ptr;
  }

  raw_ptr<FakeQuickPairProcessManager, DanglingUntriaged> fake_process_manager_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{/*use_default_devices_and_services=*/true};
  scoped_refptr<FakeFastPairScanner> scanner_;
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<FastPairDiscoverableScannerImpl> discoverable_scanner_;
  std::unique_ptr<FastPairDiscoverableScanner> discoverable_scanner2_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  std::unique_ptr<QuickPairProcessManager> process_manager_;
  base::MockCallback<DeviceCallback> found_device_callback_;
  base::MockCallback<DeviceCallback> lost_device_callback_;
};

TEST_F(FastPairDiscoverableScannerImplTest,
       UtilityProcessStopped_FailedAllRetryAttempts) {
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  fake_process_manager_->SetProcessStopped(true);
  scanner_->NotifyDeviceFound(device);
}

TEST_F(FastPairDiscoverableScannerImplTest, UtilityProcessStopped_DeviceLost) {
  auto device = std::make_unique<device::MockBluetoothDevice>(
      adapter_.get(), 0, "test_name", kAddress, /*paired=*/false,
      /*connected=*/false);
  device->SetServiceDataForUUID(kFastPairBluetoothUuid, {1, 2, 3});

  device::BluetoothDevice* device_ptr = device.get();

  adapter_->AddMockDevice(std::move(device));
  ON_CALL(*adapter_, GetDevice(kAddress))
      .WillByDefault(testing::Return(nullptr));

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  fake_process_manager_->SetProcessStopped(true);
  scanner_->NotifyDeviceFound(device_ptr);
}

TEST_F(FastPairDiscoverableScannerImplTest, ValidModelId_FactoryCreate) {
  discoverable_scanner_.reset();
  std::unique_ptr<FastPairDiscoverableScanner>
      discoverable_scanner_from_factory =
          FastPairDiscoverableScannerImpl::Factory::Create(
              scanner_, adapter_, found_device_callback_.Get(),
              lost_device_callback_.Get());

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, NoServiceData) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  std::unique_ptr<device::BluetoothDevice> device =
      base::WrapUnique(static_cast<device::BluetoothDevice*>(
          new testing::NiceMock<device::MockBluetoothDevice>(
              adapter_.get(), 0, "test_name", "test_address",
              /*paired=*/false,
              /*connected=*/false)));

  scanner_->NotifyDeviceFound(device.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, NoModelIdDataInRepository) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  auto device = std::make_unique<device::MockBluetoothDevice>(
      adapter_.get(), 0, "test_name", kAddress, /*paired=*/false,
      /*connected=*/false);
  device->SetServiceDataForUUID(kFastPairBluetoothUuid, {1, 2, 3});
  device::BluetoothDevice* device_ptr = device.get();

  adapter_->AddMockDevice(std::move(device));
  ON_CALL(*adapter_, GetDevice(kAddress))
      .WillByDefault(testing::Return(device_ptr));

  scanner_->NotifyDeviceFound(device_ptr);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, NoMetadata) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice("");
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, ValidModelId) {
  EXPECT_CALL(found_device_callback_, Run).Times(1);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, WrongDeviceType) {
  nearby::fastpair::Device metadata;
  metadata.set_trigger_distance(2);
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);
  metadata.set_device_type(nearby::fastpair::DeviceType::AUTOMOTIVE);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, UnspecifiedNotificationType) {
  // Set metadata to mimic a device that doesn't specify the notification type,
  // interaction type, or device type. Since we aren't sure what this device is,
  // we'll show the notification to be safe.
  nearby::fastpair::Device metadata;
  metadata.set_trigger_distance(2);
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);
  metadata.set_device_type(
      nearby::fastpair::DeviceType::DEVICE_TYPE_UNSPECIFIED);
  metadata.set_notification_type(
      nearby::fastpair::NotificationType::NOTIFICATION_TYPE_UNSPECIFIED);
  metadata.set_interaction_type(
      nearby::fastpair::InteractionType::INTERACTION_TYPE_UNKNOWN);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, V1NotificationType) {
  // Set metadata to mimic a V1 device which advertises with no device
  // type, interaction type of notification, and a notification type of
  // FAST_PAIR_ONE.
  nearby::fastpair::Device metadata;
  metadata.set_trigger_distance(2);
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);
  metadata.set_device_type(
      nearby::fastpair::DeviceType::DEVICE_TYPE_UNSPECIFIED);
  metadata.set_notification_type(
      nearby::fastpair::NotificationType::FAST_PAIR_ONE);
  metadata.set_interaction_type(
      nearby::fastpair::InteractionType::NOTIFICATION);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, V2NotificationType) {
  // Set metadata to mimic a V2 device which advertises with a device
  // type of TRUE_WIRELESS_HEADPHONES, interaction type of notification, and a
  // notification type of FAST_PAIR.
  nearby::fastpair::Device metadata;
  metadata.set_trigger_distance(2);
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);
  metadata.set_device_type(
      nearby::fastpair::DeviceType::TRUE_WIRELESS_HEADPHONES);
  metadata.set_notification_type(nearby::fastpair::NotificationType::FAST_PAIR);
  metadata.set_interaction_type(
      nearby::fastpair::InteractionType::NOTIFICATION);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, WrongNotificationType) {
  // Set metadata to mimic a Fitbit wearable which advertises with no
  // device type, interaction type of notification, and a notification type of
  // APP_LAUNCH.
  nearby::fastpair::Device metadata;
  metadata.set_trigger_distance(2);
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);
  metadata.set_device_type(
      nearby::fastpair::DeviceType::DEVICE_TYPE_UNSPECIFIED);
  metadata.set_notification_type(
      nearby::fastpair::NotificationType::APP_LAUNCH);
  metadata.set_interaction_type(
      nearby::fastpair::InteractionType::NOTIFICATION);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, WrongInteractionType) {
  // Set metadata to mimic a Smart Setup advertisement which advertises with
  // no device type, interaction type of AUTO_LAUNCH, and a notification type of
  // FAST_PAIR_ONE.
  nearby::fastpair::Device metadata;
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);
  metadata.set_trigger_distance(2);
  metadata.set_device_type(
      nearby::fastpair::DeviceType::DEVICE_TYPE_UNSPECIFIED);
  metadata.set_notification_type(
      nearby::fastpair::NotificationType::FAST_PAIR_ONE);
  metadata.set_interaction_type(nearby::fastpair::InteractionType::AUTO_LAUNCH);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, MouseDisallowedWhenHIDDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairHID});
  nearby::fastpair::Device metadata;
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);
  metadata.set_trigger_distance(2);
  metadata.set_device_type(nearby::fastpair::DeviceType::MOUSE);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, MouseAllowedWhenHIDEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairHID,
                            floss::features::kFlossEnabled},
      /*disabled_features=*/{});
  nearby::fastpair::Device metadata;
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);
  metadata.set_trigger_distance(2);
  metadata.set_device_type(nearby::fastpair::DeviceType::MOUSE);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest,
       InputDeviceDisallowedWhenKeyboardsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairKeyboards});
  nearby::fastpair::Device metadata;
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);
  metadata.set_trigger_distance(2);
  metadata.set_device_type(nearby::fastpair::DeviceType::INPUT_DEVICE);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest,
       InputDeviceAllowedWhenKeyboardsEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairKeyboards,
                            floss::features::kFlossEnabled},
      /*disabled_features=*/{});
  nearby::fastpair::Device metadata;
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);
  metadata.set_trigger_distance(2);
  metadata.set_device_type(nearby::fastpair::DeviceType::INPUT_DEVICE);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, PublishedStatusTypeShown) {
  // Set metadata to mimic a device with a published Status Type.
  nearby::fastpair::Device metadata;
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);
  metadata.set_trigger_distance(2);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  // A DeviceFound notification should be displayed.
  EXPECT_CALL(found_device_callback_, Run).Times(1);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, UnpublishedStatusTypeHidden) {
  // Set metadata to mimic a device that doesn't specify the status type,
  // which is what a "Debug device" will look like.
  nearby::fastpair::Device metadata;
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::TYPE_UNSPECIFIED);
  metadata.set_trigger_distance(2);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  // No notification should be displayed.
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest,
       UnpublishedStatusTypeShownWithDebugMetadataFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairDebugMetadata},
      /*disabled_features=*/{});

  // Set metadata to mimic a device that has submitted to the Nearby console and
  // is awaiting publication.
  nearby::fastpair::Device metadata;
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::SUBMITTED);
  metadata.set_trigger_distance(2);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  // The notification should be displayed since Debug Metadata flag is on.
  EXPECT_CALL(found_device_callback_, Run).Times(1);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, DeviceLost) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  scanner_->NotifyDeviceLost(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, NearbyShareModelId) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice("fc128e");
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, InvokesLostCallbackAfterFound_v1) {
  device::BluetoothDevice* device = GetDevice(kValidModelId);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceLost(device);

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest,
       InvokesFoundCallback_AfterNetworkAvailable) {
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  repository_->set_is_network_connected(false);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  repository_->set_is_network_connected(true);
  discoverable_scanner_->DefaultNetworkChanged(
      helper_.network_state_handler()->DefaultNetwork());
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest,
       NoFoundCallback_AfterNetworkUnavailable) {
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  repository_->set_is_network_connected(false);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  repository_->set_is_network_connected(false);
  discoverable_scanner_->DefaultNetworkChanged(
      helper_.network_state_handler()->DefaultNetwork());
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest,
       NoFoundCallback_AfterDeviceLostAndNetworkAvailable) {
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  repository_->set_is_network_connected(false);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();

  scanner_->NotifyDeviceLost(device);
  repository_->set_is_network_connected(true);
  discoverable_scanner_->DefaultNetworkChanged(
      helper_.network_state_handler()->DefaultNetwork());
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest, InvokesLostCallbackAfterFound_v2) {
  nearby::fastpair::Device metadata;
  metadata.set_trigger_distance(2);
  nearby::fastpair::Status* status = metadata.mutable_status();
  status->set_status_type(nearby::fastpair::StatusType::PUBLISHED);
  metadata.set_device_type(
      nearby::fastpair::DeviceType::TRUE_WIRELESS_HEADPHONES);
  auto* key_pair = new ::nearby::fastpair::AntiSpoofingKeyPair();
  key_pair->set_public_key("test_public_key");
  metadata.set_allocated_anti_spoofing_key_pair(key_pair);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  device::BluetoothDevice* device = GetDevice(kValidModelId);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceFound(device);

  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceLost(device);

  base::RunLoop().RunUntilIdle();
}

// TODO(b/242100708): This test is misleading since we don't actually
// catch the cases where a paired device is discovered by this scanner.
// Update/remove this test once this bug is fixed.
TEST_F(FastPairDiscoverableScannerImplTest, AlreadyPaired) {
  device::BluetoothDevice* device =
      GetDevice(kValidModelId, /*is_paired=*/true);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceLost(device);

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerImplTest,
       DoesntInvokeLostCallbackIfDidntInvokeFound) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceLost(device);
  base::RunLoop().RunUntilIdle();
}

}  // namespace quick_pair
}  // namespace ash
