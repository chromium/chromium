// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner.h"
#include <memory>
#include <vector>
#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/quick_pair/scanning/fast_pair/fake_fast_pair_scanner.h"
#include "ash/quick_pair/scanning/range_tracker.h"
#include "ash/services/quick_pair/fast_pair_data_parser.h"
#include "ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kValidModelId[] = "718c17";
}  // namespace

namespace ash {
namespace quick_pair {

class FastPairDiscoverableScannerTest : public testing::Test {
 public:
  void SetUp() override {
    repository_ = std::make_unique<FakeFastPairRepository>();

    nearby::fastpair::Device metadata;
    metadata.set_trigger_distance(2);
    repository_->SetFakeMetadata(kValidModelId, metadata);

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

    discoverable_scanner_ = std::make_unique<FastPairDiscoverableScanner>(
        scanner_,
        std::make_unique<RangeTracker>(
            static_cast<scoped_refptr<device::BluetoothAdapter>>(adapter_)),
        found_device_callback_.Get(), lost_device_callback_.Get());
  }

  MockQuickPairProcessManager* mock_process_manager() {
    return static_cast<MockQuickPairProcessManager*>(process_manager_.get());
  }

 protected:
  std::unique_ptr<device::BluetoothDevice> GetInRangeDevice(
      const std::string& hex_model_id,
      bool expect_call) {
    testing::NiceMock<device::MockBluetoothDevice>* device =
        new testing::NiceMock<device::MockBluetoothDevice>(
            adapter_.get(), 0, "test_name", "test_address", /*paired=*/false,
            /*connected=*/false);

    if (!hex_model_id.empty()) {
      std::vector<uint8_t> model_id_bytes;
      base::HexStringToBytes(hex_model_id, &model_id_bytes);
      device->SetServiceDataForUUID(kFastPairBluetoothUuid, model_id_bytes);

      EXPECT_CALL(*device, GetInquiryRSSI)
          .Times(expect_call ? 1 : 0)
          .WillOnce(testing::Return(-80));
      EXPECT_CALL(*device, GetInquiryTxPower)
          .Times(expect_call ? 1 : 0)
          .WillOnce(testing::Return(-40));
      ;
    }

    return base::WrapUnique(static_cast<device::BluetoothDevice*>(device));
  }

  base::test::SingleThreadTaskEnvironment task_enviornment_;
  scoped_refptr<FakeFastPairScanner> scanner_;
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<FastPairDiscoverableScanner> discoverable_scanner_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  std::unique_ptr<QuickPairProcessManager> process_manager_;
  mojo::SharedRemote<mojom::FastPairDataParser> data_parser_remote_;
  mojo::PendingRemote<mojom::FastPairDataParser> fast_pair_data_parser_;
  std::unique_ptr<FastPairDataParser> data_parser_;
  base::MockCallback<DeviceCallback> found_device_callback_;
  base::MockCallback<DeviceCallback> lost_device_callback_;
};

TEST_F(FastPairDiscoverableScannerTest, NoServiceData) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  std::unique_ptr<device::BluetoothDevice> device =
      base::WrapUnique(static_cast<device::BluetoothDevice*>(
          new testing::NiceMock<device::MockBluetoothDevice>(
              adapter_.get(), 0, "test_name", "test_address", /*paired=*/false,
              /*connected=*/false)));

  scanner_->NotifyDeviceFound(device.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest, NoModelId) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  std::unique_ptr<device::BluetoothDevice> device =
      GetInRangeDevice("", /*expect_call=*/false);
  scanner_->NotifyDeviceFound(device.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest, ValidModelId) {
  EXPECT_CALL(found_device_callback_, Run).Times(1);
  std::unique_ptr<device::BluetoothDevice> device =
      GetInRangeDevice(kValidModelId, /*expect_call=*/true);
  scanner_->NotifyDeviceFound(device.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest, NearbyShareModelId) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  std::unique_ptr<device::BluetoothDevice> device =
      GetInRangeDevice("fc128e", /*expect_call=*/false);
  scanner_->NotifyDeviceFound(device.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest, InvokesLostCallbackAfterFound) {
  std::unique_ptr<device::BluetoothDevice> device =
      GetInRangeDevice(kValidModelId, /*expect_call=*/true);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceFound(device.get());

  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceLost(device.get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest,
       DoesntInvokeLostCallbackIfDidntInvokeFound) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  std::unique_ptr<device::BluetoothDevice> device =
      GetInRangeDevice(kValidModelId, /*expect_call=*/false);
  scanner_->NotifyDeviceLost(device.get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace quick_pair
}  // namespace ash
