// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/hid_battery_listener.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace ash {
namespace {

const char kPath[] = "/sys/class/power_supply/hid-AA:BB:CC:DD:EE:FF-battery";
const char kAddress[] = "ff:ee:dd:cc:bb:aa";
const char kDeviceName[] = "test device";

}  // namespace

class HidBatteryListenerTest : public AshTestBase {
 public:
  HidBatteryListenerTest() = default;

  ~HidBatteryListenerTest() override = default;

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

    listener_ = std::make_unique<HidBatteryListener>(mock_adapter_);
  }

  void TearDown() override {
    listener_.reset();
    AshTestBase::TearDown();
  }

  void NotifyPeripheralBatteryStatusReceived(const std::string& path,
                                             const std::string& name,
                                             int level) {
    listener_->PeripheralBatteryStatusReceived(path, name, level);
  }

 protected:
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_;
  std::unique_ptr<HidBatteryListener> listener_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HidBatteryListenerTest);
};

TEST_F(HidBatteryListenerTest, SetsBatteryToDevice) {
  NotifyPeripheralBatteryStatusReceived(kPath, kDeviceName, 100);
  EXPECT_EQ(100, mock_device_->battery_percentage());
  NotifyPeripheralBatteryStatusReceived(kPath, kDeviceName, 0);
  EXPECT_EQ(0, mock_device_->battery_percentage());
  NotifyPeripheralBatteryStatusReceived(kPath, kDeviceName, 55);
  EXPECT_EQ(55, mock_device_->battery_percentage());
}

TEST_F(HidBatteryListenerTest, InvalidBatteryLevel) {
  NotifyPeripheralBatteryStatusReceived(kPath, kDeviceName, 101);
  EXPECT_EQ(base::nullopt, mock_device_->battery_percentage());
  NotifyPeripheralBatteryStatusReceived(kPath, kDeviceName, 200);
  EXPECT_EQ(base::nullopt, mock_device_->battery_percentage());
  NotifyPeripheralBatteryStatusReceived(kPath, kDeviceName, -1);
  EXPECT_EQ(base::nullopt, mock_device_->battery_percentage());
  NotifyPeripheralBatteryStatusReceived(kPath, kDeviceName, -100);
  EXPECT_EQ(base::nullopt, mock_device_->battery_percentage());
}

TEST_F(HidBatteryListenerTest, InvalidHIDAddress) {
  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(0);

  NotifyPeripheralBatteryStatusReceived("invalid-path", kDeviceName, 100);
  NotifyPeripheralBatteryStatusReceived("/sys/class/power_supply/hid-battery",
                                        kDeviceName, 100);

  // 3 characters at the end of the address, "f55".
  NotifyPeripheralBatteryStatusReceived(
      "/sys/class/power_supply/hid-A0:b1:C2:d3:E4:f55-battery", kDeviceName,
      100);

  // 3 characters at the start of the address, "A00".
  NotifyPeripheralBatteryStatusReceived(
      "/sys/class/power_supply/hid-A00:b1:C2:d3:E4:f5-battery", kDeviceName,
      100);
}

TEST_F(HidBatteryListenerTest, DeviceNotPresentInAdapter) {
  ON_CALL(*mock_adapter_, GetDevice(kAddress)).WillByDefault(Return(nullptr));

  // Should not crash.
  NotifyPeripheralBatteryStatusReceived(kPath, kDeviceName, 100);
}

}  // namespace ash
