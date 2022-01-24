// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_not_discoverable_scanner.h"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_service_data_creator.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair/pairing_metadata.h"
#include "ash/quick_pair/scanning/fast_pair/fake_fast_pair_scanner.h"
#include "ash/quick_pair/scanning/range_tracker.h"
#include "ash/services/quick_pair/fast_pair_data_parser.h"
#include "ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
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
constexpr int kSaltHeader = 0b00010001;
constexpr long kModelIdLong = 7441431;
const std::string kModelIdString = "718c17";
const std::string kAccountKeyFilter = "112233445566";
const std::string kSalt = "01";
}  // namespace

namespace ash {
namespace quick_pair {

class FastPairNotDiscoverableScannerTest : public testing::Test {
 public:
  void SetUp() override {
    repository_ = std::make_unique<FakeFastPairRepository>();

    scanner_ = base::MakeRefCounted<FakeFastPairScanner>();

    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

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

    not_discoverable_scanner_ =
        std::make_unique<FastPairNotDiscoverableScanner>(
            scanner_,
            std::make_unique<RangeTracker>(
                static_cast<scoped_refptr<device::BluetoothAdapter>>(adapter_)),
            found_device_callback_.Get(), lost_device_callback_.Get());
  }

 protected:
  MockQuickPairProcessManager* mock_process_manager() {
    return static_cast<MockQuickPairProcessManager*>(process_manager_.get());
  }

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

  std::unique_ptr<device::BluetoothDevice> GetInRangeDevice(
      const std::vector<uint8_t>& service_data,
      bool expect_call) {
    testing::NiceMock<device::MockBluetoothDevice>* device =
        new testing::NiceMock<device::MockBluetoothDevice>(
            adapter_.get(), 0, "test_name", "test_address", /*paired=*/false,
            /*connected=*/false);

    device->SetServiceDataForUUID(kFastPairBluetoothUuid, service_data);

    if (expect_call) {
      EXPECT_CALL(*device, GetInquiryRSSI).WillOnce(testing::Return(-80));
      EXPECT_CALL(*device, GetInquiryTxPower).WillOnce(testing::Return(-40));
    } else {
      EXPECT_CALL(*device, GetInquiryRSSI).Times(0);
      EXPECT_CALL(*device, GetInquiryTxPower).Times(0);
    }

    return base::WrapUnique(static_cast<device::BluetoothDevice*>(device));
  }

  base::test::SingleThreadTaskEnvironment task_enviornment_;
  scoped_refptr<FakeFastPairScanner> scanner_;
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<FastPairNotDiscoverableScanner> not_discoverable_scanner_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  std::unique_ptr<QuickPairProcessManager> process_manager_;
  mojo::SharedRemote<mojom::FastPairDataParser> data_parser_remote_;
  mojo::PendingRemote<mojom::FastPairDataParser> fast_pair_data_parser_;
  std::unique_ptr<FastPairDataParser> data_parser_;
  base::MockCallback<DeviceCallback> found_device_callback_;
  base::MockCallback<DeviceCallback> lost_device_callback_;
};

TEST_F(FastPairNotDiscoverableScannerTest, NoServiceData) {
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

TEST_F(FastPairNotDiscoverableScannerTest, NoParsedAdvertisement) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  std::unique_ptr<device::BluetoothDevice> device =
      GetInRangeDevice(GetDiscoverableAdvServicedata(),
                       /*expect_call=*/false);
  scanner_->NotifyDeviceFound(device.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerTest, DontShowUI) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  std::unique_ptr<device::BluetoothDevice> device =
      GetInRangeDevice(GetAdvNoUiServicedata(), /*expect_call=*/false);
  scanner_->NotifyDeviceFound(device.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerTest, NoMetadata) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  std::unique_ptr<device::BluetoothDevice> device =
      GetInRangeDevice(GetAdvServicedata(), /*expect_call=*/false);
  scanner_->NotifyDeviceFound(device.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerTest, InvokesLostCallbackAfterFound) {
  std::unique_ptr<device::BluetoothDevice> device =
      GetInRangeDevice(GetAdvServicedata(), /*expect_call=*/true);

  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kModelIdLong);
  response.mutable_device()->set_trigger_distance(2);

  auto device_metadata =
      std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());
  PairingMetadata pairing_metadata(device_metadata.get(),
                                   std::vector<uint8_t>());
  repository_->SetCheckAccountKeysResult(pairing_metadata);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceFound(device.get());

  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceLost(device.get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairNotDiscoverableScannerTest,
       DoesntInvokeLostCallbackIfDidntInvokeFound) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  std::unique_ptr<device::BluetoothDevice> device =
      GetInRangeDevice(std::vector<uint8_t>(), /*expect_call=*/false);
  scanner_->NotifyDeviceLost(device.get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace quick_pair
}  // namespace ash
