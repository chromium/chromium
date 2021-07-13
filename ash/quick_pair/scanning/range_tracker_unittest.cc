// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/range_tracker.h"

#include <cstdint>
#include <memory>

#include "base/test/mock_callback.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  void NotifiyDeviceChanged(device::BluetoothDevice* device) {
    device::BluetoothAdapter::NotifyDeviceChanged(device);
  }

 private:
  ~FakeBluetoothAdapter() = default;
};

}  // namespace

namespace ash {
namespace quick_pair {

class RangeTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    tracker_ = std::make_unique<RangeTracker>(
        static_cast<scoped_refptr<device::BluetoothAdapter>>(adapter_));
    device_ = std::make_unique<device::MockBluetoothDevice>(
        adapter_.get(),
        /*bluetooth_class=*/0, "Test device name", "Test address",
        /*paired=*/false,
        /*connected=*/false);
  }

 protected:
  void SetRssiAndTxPower(int8_t rssi, int8_t tx_power) {
    EXPECT_CALL(*(device_.get()), GetInquiryRSSI)
        .WillOnce(testing::Return(rssi));
    EXPECT_CALL(*(device_.get()), GetInquiryTxPower)
        .WillOnce(testing::Return(tx_power));
  }

  std::unique_ptr<device::MockBluetoothDevice> device_;
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  std::unique_ptr<RangeTracker> tracker_;
  base::MockCallback<base::OnceCallback<void(device::BluetoothDevice*)>>
      callback_;
};

TEST_F(RangeTrackerTest, DeviceAlreadyInRange) {
  SetRssiAndTxPower(-88, -40);
  EXPECT_CALL(callback_, Run(device_.get()));
  tracker_->Track(device_.get(), 3, callback_.Get());
}

TEST_F(RangeTrackerTest, DeviceAlreadyInRange_KnownTxPower) {
  EXPECT_CALL(*(device_.get()), GetInquiryRSSI).WillOnce(testing::Return(-88));
  EXPECT_CALL(*(device_.get()), GetInquiryTxPower).Times(0);
  EXPECT_CALL(callback_, Run(device_.get()));
  tracker_->Track(device_.get(), 3, callback_.Get(), -40);
}

TEST_F(RangeTrackerTest, DeviceNotInRange) {
  SetRssiAndTxPower(-88, -40);
  EXPECT_CALL(callback_, Run(device_.get())).Times(0);
  tracker_->Track(device_.get(), 2, callback_.Get());
}

TEST_F(RangeTrackerTest, DeviceComesIntoRange) {
  SetRssiAndTxPower(-100, -40);
  EXPECT_CALL(callback_, Run(device_.get())).Times(0);
  tracker_->Track(device_.get(), 3, callback_.Get());

  SetRssiAndTxPower(-88, -40);
  EXPECT_CALL(callback_, Run(device_.get()));
  adapter_->NotifiyDeviceChanged(device_.get());
}

TEST_F(RangeTrackerTest, DoesntCallbackForUntrackedDevice) {
  SetRssiAndTxPower(-88, -40);
  EXPECT_CALL(callback_, Run(device_.get())).Times(0);
  tracker_->Track(device_.get(), 2, callback_.Get());

  std::unique_ptr<device::MockBluetoothDevice> untracked_device =
      std::make_unique<device::MockBluetoothDevice>(
          adapter_.get(),
          /*bluetooth_class=*/0, "Untracked device", "Untracked address",
          /*paired=*/false,
          /*connected=*/false);

  EXPECT_CALL(*(untracked_device.get()), GetInquiryRSSI).Times(0);
  EXPECT_CALL(*(untracked_device.get()), GetInquiryTxPower).Times(0);
  EXPECT_CALL(callback_, Run).Times(0);

  adapter_->NotifiyDeviceChanged(untracked_device.get());
}

TEST_F(RangeTrackerTest, StopTracking) {
  SetRssiAndTxPower(-100, -40);
  EXPECT_CALL(callback_, Run(device_.get())).Times(0);
  tracker_->Track(device_.get(), 3, callback_.Get());

  EXPECT_CALL(callback_, Run).Times(0);
  EXPECT_CALL(*(device_.get()), GetInquiryRSSI).Times(0);
  EXPECT_CALL(*(device_.get()), GetInquiryTxPower).Times(0);

  tracker_->StopTracking(device_.get());
  adapter_->NotifiyDeviceChanged(device_.get());
}

}  // namespace quick_pair
}  // namespace ash
