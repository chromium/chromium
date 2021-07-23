// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner.h"
#include <memory>
#include <vector>
#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/scanning/fast_pair/fake_fast_pair_scanner.h"
#include "ash/quick_pair/scanning/range_tracker.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kValidModelId[] = "718c17";
}  // namespace

namespace ash {
namespace quick_pair {

class FastPairDiscoverableScannerTest : public testing::Test {
 public:
  void SetUp() override {
    scanner_ = base::MakeRefCounted<FakeFastPairScanner>();

    adapter_ = base::MakeRefCounted<device::MockBluetoothAdapter>();

    discoverable_scanner_ = std::make_unique<FastPairDiscoverableScanner>(
        scanner_,
        std::make_unique<RangeTracker>(
            static_cast<scoped_refptr<device::BluetoothAdapter>>(adapter_)),
        found_device_callback_.Get(), lost_device_callback_.Get());
  }

 protected:
  std::unique_ptr<device::BluetoothDevice> GetInRangeDevice(
      const std::string& hex_model_id,
      bool expect_call) {
    device::MockBluetoothDevice* device = new device::MockBluetoothDevice(
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

  scoped_refptr<FakeFastPairScanner> scanner_;
  std::unique_ptr<FastPairDiscoverableScanner> discoverable_scanner_;
  scoped_refptr<device::MockBluetoothAdapter> adapter_;
  base::MockCallback<DeviceCallback> found_device_callback_;
  base::MockCallback<DeviceCallback> lost_device_callback_;
};

TEST_F(FastPairDiscoverableScannerTest, NoModelId) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(
      GetInRangeDevice("", /*expect_call=*/false).get());
}

TEST_F(FastPairDiscoverableScannerTest, ValidModelId) {
  EXPECT_CALL(found_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceFound(
      GetInRangeDevice(kValidModelId, /*expect_call=*/true).get());
}

TEST_F(FastPairDiscoverableScannerTest, NearbyShareModelId) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(
      GetInRangeDevice("fc128e", /*expect_call=*/false).get());
}

TEST_F(FastPairDiscoverableScannerTest, InvokesLostCallbackAfterFound) {
  std::unique_ptr<device::BluetoothDevice> device =
      GetInRangeDevice(kValidModelId, /*expect_call=*/true);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceFound(device.get());

  EXPECT_CALL(lost_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceLost(device.get());
}

TEST_F(FastPairDiscoverableScannerTest,
       DoesntInvokeLostCallbackIfDidntInvokeFound) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceLost(
      GetInRangeDevice(kValidModelId, /*expect_call=*/false).get());
}

}  // namespace quick_pair
}  // namespace ash
