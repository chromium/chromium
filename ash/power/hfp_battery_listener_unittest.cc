// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/hfp_battery_listener.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace ash {
namespace {

const char kAddress[] = "ff:ee:dd:cc:bb:aa";
const char kDeviceName[] = "test device";

}  // namespace

class HfpBatteryListenerTest : public AshTestBase {
 public:
  HfpBatteryListenerTest() = default;

  ~HfpBatteryListenerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();

    mock_device_ = std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        mock_adapter_.get(), 0 /* bluetooth_class */, kDeviceName, kAddress,
        true /* paired */, true /* connected */);
    ASSERT_FALSE(mock_device_->battery_percentage());
    ON_CALL(*mock_adapter_, GetDevice(kAddress))
        .WillByDefault(Return(mock_device_.get()));

    // Check that HfpBatteryListener subscribes to be adapter observer.
    EXPECT_CALL(*mock_adapter_, AddObserver(_))
        .WillOnce(testing::SaveArg<0>(&adapter_observer_));

    listener_ = std::make_unique<HfpBatteryListener>(mock_adapter_);
  }

  void TearDown() override {
    listener_.reset();
    AshTestBase::TearDown();
  }

  void NotifyBluetoothBatteryLevelChangedReceived(const std::string& address,
                                                  uint32_t level) {
    listener_->OnBluetoothBatteryChanged(address, level);
  }

 protected:
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_;
  std::unique_ptr<HfpBatteryListener> listener_;
  device::BluetoothAdapter::Observer* adapter_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HfpBatteryListenerTest);
};

TEST_F(HfpBatteryListenerTest, SetsBatteryToDevice) {
  NotifyBluetoothBatteryLevelChangedReceived(kAddress, 100);
  EXPECT_EQ(100, mock_device_->battery_percentage());
  NotifyBluetoothBatteryLevelChangedReceived(kAddress, 0);
  EXPECT_EQ(0, mock_device_->battery_percentage());
  NotifyBluetoothBatteryLevelChangedReceived(kAddress, 55);
  EXPECT_EQ(55, mock_device_->battery_percentage());
}

TEST_F(HfpBatteryListenerTest, InvalidBatteryLevel) {
  NotifyBluetoothBatteryLevelChangedReceived(kAddress, 101);
  EXPECT_EQ(base::nullopt, mock_device_->battery_percentage());
  NotifyBluetoothBatteryLevelChangedReceived(kAddress, 200);
  EXPECT_EQ(base::nullopt, mock_device_->battery_percentage());
}

TEST_F(HfpBatteryListenerTest, DeviceNotPresentInAdapter) {
  ON_CALL(*mock_adapter_, GetDevice(kAddress)).WillByDefault(Return(nullptr));

  // Should not crash.
  NotifyBluetoothBatteryLevelChangedReceived(kAddress, 100);
}

}  // namespace ash
