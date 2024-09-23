// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_not_discoverable_scanner_impl.h"

#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_service_data_creator.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair/pairing_metadata.h"
#include "ash/quick_pair/scanning/fast_pair/fake_fast_pair_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_not_discoverable_scanner.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/quick_pair/fast_pair_data_parser.h"
#include "chromeos/ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr int kNotDiscoverableAdvHeader = 0b00000110;
constexpr int kAccountKeyFilterHeader = 0b01100000;
constexpr int kAccountKeyFilterNoNotificationHeader = 0b01100010;
constexpr int kBatteryHeader = 0b00110011;
constexpr int kSaltHeader = 0b00010001;
constexpr long kModelIdLong = 7441431;
const std::string kModelIdString = "718c17";
const std::string kAccountKeyFilter = "112233445566";
const std::string kSalt = "01";
const std::string kBattery = "01048F";
const std::string kInvalidBattery = "F1F48F";
const std::string kBatteryUnknownValue = "3FFFFF";
const std::string kModelId = "112233";
const std::string kAddress = "test_address";

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairNotDiscoverableScannerImplTest : public testing::Test {
 public:
  void SetUp() override {
    repository_ = std::make_unique<FakeFastPairRepository>();

    scanner_ = base::MakeRefCounted<FakeFastPairScanner>();

    data_parser_ = std::make_unique<ash::quick_pair::FastPairDataParser>(
        fast_pair_data_parser_.InitWithNewPipeAndPassReceiver());

    data_parser_remote_.Bind(std::move(fast_pair_data_parser_),
                             task_enviornment_.GetMainThreadTaskRunner());

    process_manager_ = std::make_unique<MockQuickPairProcessManager>();
    quick_pair_process::SetProcessManager(process_manager_.get());

    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    not_discoverable_scanner_ =
        std::make_unique<FastPairNotDiscoverableScannerImpl>(
            scanner_, adapter_, found_device_callback_.Get(),
            lost_device_callback_.Get());
  }

 protected:
  std::vector<uint8_t> GetDiscoverableAdvServicedata() {
    std::vector<uint8_t> model_id_bytes;
    base::HexStringToBytes(kModelIdString, &model_id_bytes);
    return model_id_bytes;
  }

  std::vector<uint8_t> GetAdvNoUiServicedata() {
    return FastPairServiceDataCreator::Builder()
        .SetHeader(kNotDiscoverableAdvHeader)
        .SetModelId(kModelIdString)
        .AddExtraFieldHeader(kAccountKeyFilterNoNotificationHeader)
        .AddExtraField(kAccountKeyFilter)
        .AddExtraFieldHeader(kSaltHeader)
        .AddExtraField(kSalt)
        .Build()
        ->CreateServiceData();
  }

  std::vector<uint8_t> GetAdvServicedata() {
    return FastPairServiceDataCreator::Builder()
        .SetHeader(kNotDiscoverableAdvHeader)
        .SetModelId(kModelIdString)
        .AddExtraFieldHeader(kAccountKeyFilterHeader)
        .AddExtraField(kAccountKeyFilter)
        .AddExtraFieldHeader(kSaltHeader)
        .AddExtraField(kSalt)
        .Build()
        ->CreateServiceData();
  }

  std::vector<uint8_t> GetAdvBatteryServicedata() {
    return FastPairServiceDataCreator::Builder()
        .SetHeader(kNotDiscoverableAdvHeader)
        .SetModelId(kModelId)
        .AddExtraFieldHeader(kAccountKeyFilterHeader)
        .AddExtraField(kAccountKeyFilter)
        .AddExtraFieldHeader(kSaltHeader)
        .AddExtraField(kSalt)
        .AddExtraFieldHeader(kBatteryHeader)
        .AddExtraField(kBattery)
        .Build()
        ->CreateServiceData();
  }

  std::vector<uint8_t> GetAdvInvalidBatteryServicedata() {
    return FastPairServiceDataCreator::Builder()
        .SetHeader(kNotDiscoverableAdvHeader)
        .SetModelId(kModelId)
        .AddExtraFieldHeader(kAccountKeyFilterHeader)
        .AddExtraField(kAccountKeyFilter)
        .AddExtraFieldHeader(kSaltHeader)
        .AddExtraField(kSalt)
        .AddExtraFieldHeader(kBatteryHeader)
        .AddExtraField(kInvalidBattery)
        .Build()
        ->CreateServiceData();
  }

  std::vector<uint8_t> GetAdvUnknownBatteryServicedata() {
    return FastPairServiceDataCreator::Builder()
        .SetHeader(kNotDiscoverableAdvHeader)
        .SetModelId(kModelId)
        .AddExtraFieldHeader(kAccountKeyFilterHeader)
        .AddExtraField(kAccountKeyFilter)
        .AddExtraFieldHeader(kSaltHeader)
        .AddExtraField(kSalt)
        .AddExtraFieldHeader(kBatteryHeader)
        .AddExtraField(kBatteryUnknownValue)
        .Build()
        ->CreateServiceData();
  }

  device::BluetoothDevice* GetDevice(const std::vector<uint8_t>& service_data,
                                     bool is_paired = false) {
    auto device = std::make_unique<device::MockBluetoothDevice>(
        adapter_.get(), 0, "test_name", kAddress, /*paired=*/is_paired,
        /*connected=*/false);
    device::BluetoothDevice* device_ptr = device.get();

    device->SetServiceDataForUUID(kFastPairBluetoothUuid, service_data);

    adapter_->AddMockDevice(std::move(device));
    ON_CALL(*adapter_, GetDevice(kAddress))
        .WillByDefault(testing::Return(device_ptr));

    return device_ptr;
  }

  base::test::SingleThreadTaskEnvironment task_enviornment_;
  scoped_refptr<FakeFastPairScanner> scanner_;
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<FastPairNotDiscoverableScanner> not_discoverable_scanner_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  std::unique_ptr<MockQuickPairProcessManager> process_manager_;
  mojo::SharedRemote<mojom::FastPairDataParser> data_parser_remote_;
  mojo::PendingRemote<mojom::FastPairDataParser> fast_pair_data_parser_;
  std::unique_ptr<FastPairDataParser> data_parser_;
  base::MockCallback<DeviceCallback> found_device_callback_;
  base::MockCallback<DeviceCallback> lost_device_callback_;
};

// TODO(crbug.com/40822900) flaky on ASan + LSan bots
#if defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_UtilityProcessStopped_FailedAllRetryAttempts \
  DISABLED_UtilityProcessStopped_FailedAllRetryAttempts
#else
#define MAYBE_UtilityProcessStopped_FailedAllRetryAttempts \
  UtilityProcessStopped_FailedAllRetryAttempts
#endif
TEST_F(FastPairNotDiscoverableScannerImplTest,
       MAYBE_UtilityProcessStopped_FailedAllRetryAttempts) {
  device::BluetoothDevice* device = GetDevice(GetAdvServicedata());
  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  repository_->set_is_account_key_paired_locally(false);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            std::move(callback).Run(
                QuickPairProcessManager::ShutdownReason::kCrash);
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });
  scanner_->NotifyDeviceFound(device);
}

// TODO(crbug.com/40822900) flaky on ASan + LSan bots
#if defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_UtilityProcessStopped_DeviceLost \
  DISABLED_UtilityProcessStopped_DeviceLost
#else
#define MAYBE_UtilityProcessStopped_DeviceLost \
  UtilityProcessStopped_DeviceLost
#endif
TEST_F(FastPairNotDiscoverableScannerImplTest,
       MAYBE_UtilityProcessStopped_DeviceLost) {
  auto device = std::make_unique<device::MockBluetoothDevice>(
      adapter_.get(), 0, "test_name", kAddress, /*paired=*/false,
      /*connected=*/false);
  device->SetServiceDataForUUID(kFastPairBluetoothUuid, {1, 2, 3});

  device::BluetoothDevice* device_ptr = device.get();

  adapter_->AddMockDevice(std::move(device));
  ON_CALL(*adapter_, GetDevice(kAddress))
      .WillByDefault(testing::Return(nullptr));

  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  repository_->set_is_account_key_paired_locally(false);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            std::move(callback).Run(
                QuickPairProcessManager::ShutdownReason::kCrash);
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });
  scanner_->NotifyDeviceFound(device_ptr);
}

// TODO(crbug.com/40822900) flaky on ASan + LSan bots
#if defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_UtilityProcessStopped_FastPairServiceDataLost \
  DISABLED_UtilityProcessStopped_FastPairServiceDataLost
#else
#define MAYBE_UtilityProcessStopped_FastPairServiceDataLost \
  UtilityProcessStopped_FastPairServiceDataLost
#endif
TEST_F(FastPairNotDiscoverableScannerImplTest,
       MAYBE_UtilityProcessStopped_FastPairServiceDataLost) {
  auto device = std::make_unique<device::MockBluetoothDevice>(
      adapter_.get(), 0, "test_name", kAddress, /*paired=*/false,
      /*connected=*/false);
  device::BluetoothDevice* device_ptr = device.get();

  auto* mock_device = static_cast<device::MockBluetoothDevice*>(device_ptr);

  device->SetServiceDataForUUID(kFastPairBluetoothUuid, GetAdvServicedata());

  adapter_->AddMockDevice(std::move(device));
  ON_CALL(*adapter_, GetDevice(kAddress))
      .WillByDefault(testing::Return(device_ptr));

  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  repository_->set_is_account_key_paired_locally(false);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            std::move(callback).Run(
                QuickPairProcessManager::ShutdownReason::kCrash);
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });
  scanner_->NotifyDeviceFound(device_ptr);
  mock_device->SetServiceDataForUUID(kFastPairBluetoothUuid, {});
}

TEST_F(FastPairNotDiscoverableScannerImplTest, NoServiceData) {
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

TEST_F(FastPairNotDiscoverableScannerImplTest, NoParsedAdvertisement) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice(GetDiscoverableAdvServicedata());
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerImplTest, DontShowUI) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice(GetAdvNoUiServicedata());
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerImplTest, NoMetadata) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice(GetAdvServicedata());
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerImplTest, DeviceLostDuringParsing) {
  device::BluetoothDevice* device = GetDevice(GetAdvServicedata());

  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  repository_->set_is_account_key_paired_locally(false);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(device);
  scanner_->NotifyDeviceLost(device);

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerImplTest, NoModelId) {
  device::BluetoothDevice* device = GetDevice(GetAdvServicedata());

  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  repository_->set_is_account_key_paired_locally(false);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(device);

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerImplTest, InvokesLostCallbackAfterFound) {
  device::BluetoothDevice* device = GetDevice(GetAdvServicedata());

  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  repository_->set_is_account_key_paired_locally(false);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceFound(device);

  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceLost(device);

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerImplTest, AlreadySavedToChromebook) {
  device::BluetoothDevice* device = GetDevice(GetAdvServicedata());

  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  repository_->set_is_account_key_paired_locally(true);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(device);

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerImplTest, FactoryCreate) {
  not_discoverable_scanner_.reset();
  std::unique_ptr<FastPairNotDiscoverableScanner>
      discoverable_scanner_from_factory =
          FastPairNotDiscoverableScannerImpl::Factory::Create(
              scanner_, adapter_, found_device_callback_.Get(),
              lost_device_callback_.Get());

  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  repository_->set_is_account_key_paired_locally(false);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });

  EXPECT_CALL(found_device_callback_, Run).Times(1);

  device::BluetoothDevice* device = GetDevice(GetAdvBatteryServicedata());
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerImplTest,
       DoesntInvokeLostCallbackIfDidntInvokeFound) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });
  device::BluetoothDevice* device = GetDevice(std::vector<uint8_t>());
  scanner_->NotifyDeviceLost(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerImplTest, SetBatteryInfo) {
  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  repository_->set_is_account_key_paired_locally(false);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });

  device::BluetoothDevice* device = GetDevice(GetAdvBatteryServicedata());
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::nullopt, device->GetBatteryInfo(
                              device::BluetoothDevice::BatteryType::kDefault));
  EXPECT_NE(std::nullopt,
            device->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_NE(std::nullopt,
            device->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_NE(std::nullopt,
            device->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));
}

TEST_F(FastPairNotDiscoverableScannerImplTest, SetUnknownBatteryInfo) {
  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  repository_->set_is_account_key_paired_locally(false);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });

  device::BluetoothDevice* device =
      GetDevice(GetAdvUnknownBatteryServicedata());
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::nullopt, device->GetBatteryInfo(
                              device::BluetoothDevice::BatteryType::kDefault));
  EXPECT_NE(std::nullopt,
            device->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_NE(std::nullopt,
            device->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_NE(std::nullopt,
            device->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));
}

TEST_F(FastPairNotDiscoverableScannerImplTest, SetInvalidPercentBatteryInfo) {
  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  repository_->set_is_account_key_paired_locally(false);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });

  device::BluetoothDevice* device =
      GetDevice(GetAdvInvalidBatteryServicedata());
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::nullopt, device->GetBatteryInfo(
                              device::BluetoothDevice::BatteryType::kDefault));
  EXPECT_NE(std::nullopt,
            device->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kLeftBudTrueWireless));
  EXPECT_NE(std::nullopt,
            device->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kRightBudTrueWireless));
  EXPECT_NE(std::nullopt,
            device->GetBatteryInfo(
                device::BluetoothDevice::BatteryType::kCaseTrueWireless));
}

TEST_F(FastPairNotDiscoverableScannerImplTest, AlreadyInitialPaired) {
  device::BluetoothDevice* device =
      GetDevice(GetAdvServicedata(), /*is_paired=*/true);

  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  // After initial pairing, we'll save the account key locally.
  repository_->set_is_account_key_paired_locally(true);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceLost(device);

  base::RunLoop().RunUntilIdle();
}

// TODO(b/242891006): Re-enable this test after the bug is fixed.
TEST_F(FastPairNotDiscoverableScannerImplTest, DISABLED_AlreadyClassicPaired) {
  device::BluetoothDevice* device =
      GetDevice(GetAdvServicedata(), /*is_paired=*/true);

  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);
  // When classic pairing, the user elects to not save their account key.
  repository_->set_is_account_key_paired_locally(false);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceLost(device);

  base::RunLoop().RunUntilIdle();
}

}  // namespace quick_pair
}  // namespace ash
