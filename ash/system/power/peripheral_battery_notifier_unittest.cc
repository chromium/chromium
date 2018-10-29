// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/peripheral_battery_notifier.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "services/ws/public/cpp/input_devices/input_device_client_test_api.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

const char kTestBatteryPath[] =
    "/sys/class/power_supply/hid-AA:BB:CC:DD:EE:FF-battery";
const char kTestBatteryAddress[] = "ff:ee:dd:cc:bb:aa";
const char kTestDeviceName[] = "test device";

}  // namespace

namespace ash {

class PeripheralBatteryNotifierTest : public ash::AshTestBase {
 public:
  PeripheralBatteryNotifierTest() = default;
  ~PeripheralBatteryNotifierTest() override = default;

  void SetUp() override {
    ash::AshTestBase::SetUp();
    observer_ = std::make_unique<PeripheralBatteryNotifier>();
  }

  void TearDown() override {
    observer_.reset();
    ash::AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<PeripheralBatteryNotifier> observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PeripheralBatteryNotifierTest);
};

TEST_F(PeripheralBatteryNotifierTest, Basic) {
  base::SimpleTestTickClock clock;
  observer_->set_testing_clock(&clock);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // Level 50 at time 100, no low-battery notification.
  clock.Advance(base::TimeDelta::FromSeconds(100));
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             50);
  EXPECT_EQ(1u, observer_->batteries_.count(kTestBatteryPath));

  const PeripheralBatteryNotifier::BatteryInfo& info =
      observer_->batteries_[kTestBatteryPath];

  EXPECT_EQ(kTestDeviceName, info.name);
  EXPECT_EQ(50, info.level);
  EXPECT_EQ(base::TimeTicks(), info.last_notification_timestamp);
  EXPECT_EQ(kTestBatteryAddress, info.bluetooth_address);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(kTestBatteryPath) ==
              nullptr);

  // Level 5 at time 110, low-battery notification.
  clock.Advance(base::TimeDelta::FromSeconds(10));
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             5);
  EXPECT_EQ(5, info.level);
  EXPECT_EQ(clock.NowTicks(), info.last_notification_timestamp);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(kTestBatteryPath) !=
              nullptr);

  // Verify that the low-battery notification for stylus does not show up.
  EXPECT_FALSE(message_center->FindVisibleNotificationById(
                   PeripheralBatteryNotifier::kStylusNotificationId) !=
               nullptr);

  // Level -1 at time 115, cancel previous notification
  clock.Advance(base::TimeDelta::FromSeconds(5));
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             -1);
  EXPECT_EQ(5, info.level);
  EXPECT_EQ(clock.NowTicks() - base::TimeDelta::FromSeconds(5),
            info.last_notification_timestamp);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(kTestBatteryPath) ==
              nullptr);

  // Level 50 at time 120, no low-battery notification.
  clock.Advance(base::TimeDelta::FromSeconds(5));
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             50);
  EXPECT_EQ(50, info.level);
  EXPECT_EQ(clock.NowTicks() - base::TimeDelta::FromSeconds(10),
            info.last_notification_timestamp);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(kTestBatteryPath) ==
              nullptr);

  // Level 5 at time 130, no low-battery notification (throttling).
  clock.Advance(base::TimeDelta::FromSeconds(10));
  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             5);
  EXPECT_EQ(5, info.level);
  EXPECT_EQ(clock.NowTicks() - base::TimeDelta::FromSeconds(20),
            info.last_notification_timestamp);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(kTestBatteryPath) ==
              nullptr);
}

TEST_F(PeripheralBatteryNotifierTest, InvalidBatteryInfo) {
  const std::string invalid_path1 = "invalid-path";
  const std::string invalid_path2 = "/sys/class/power_supply/hid-battery";

  observer_->PeripheralBatteryStatusReceived(invalid_path1, kTestDeviceName,
                                             10);
  EXPECT_TRUE(observer_->batteries_.empty());

  observer_->PeripheralBatteryStatusReceived(invalid_path2, kTestDeviceName,
                                             10);
  EXPECT_TRUE(observer_->batteries_.empty());

  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             -2);
  EXPECT_TRUE(observer_->batteries_.empty());

  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             101);
  EXPECT_TRUE(observer_->batteries_.empty());

  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             -1);
  EXPECT_TRUE(observer_->batteries_.empty());
}

// Verify that for Bluetooth devices, the correct address gets stored in the
// BatteryInfo's bluetooth_address member, and for non-Bluetooth devices, that
// bluetooth_address member is empty.
TEST_F(PeripheralBatteryNotifierTest, ExtractBluetoothAddress) {
  const std::string bluetooth_path =
      "/sys/class/power_supply/hid-A0:b1:C2:d3:E4:f5-battery";
  const std::string expected_bluetooth_address = "f5:e4:d3:c2:b1:a0";
  const std::string non_bluetooth_path =
      "/sys/class/power_supply/hid-notbluetooth-battery";

  observer_->PeripheralBatteryStatusReceived(bluetooth_path, kTestDeviceName,
                                             10);
  observer_->PeripheralBatteryStatusReceived(non_bluetooth_path,
                                             kTestDeviceName, 10);
  EXPECT_EQ(2u, observer_->batteries_.size());

  const PeripheralBatteryNotifier::BatteryInfo& bluetooth_device_info =
      observer_->batteries_[bluetooth_path];
  EXPECT_EQ(expected_bluetooth_address,
            bluetooth_device_info.bluetooth_address);
  const PeripheralBatteryNotifier::BatteryInfo& non_bluetooth_device_info =
      observer_->batteries_[non_bluetooth_path];
  EXPECT_TRUE(non_bluetooth_device_info.bluetooth_address.empty());
}

// TODO(crbug.com/765794): Flaky on ash_unittests with mus.
TEST_F(PeripheralBatteryNotifierTest, DISABLED_DeviceRemove) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  observer_->PeripheralBatteryStatusReceived(kTestBatteryPath, kTestDeviceName,
                                             5);
  EXPECT_EQ(1u, observer_->batteries_.count(kTestBatteryPath));
  EXPECT_TRUE(message_center->FindVisibleNotificationById(kTestBatteryPath) !=
              nullptr);

  observer_->RemoveBluetoothBattery(kTestBatteryAddress);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(kTestBatteryPath) ==
              nullptr);
}

// TODO(crbug.com/765794): Flaky on ash_unittests with mus.
TEST_F(PeripheralBatteryNotifierTest, DISABLED_StylusNotification) {
  const std::string kTestStylusBatteryPath =
      "/sys/class/power_supply/hid-AAAA:BBBB:CCCC.DDDD-battery";
  const std::string kTestStylusName = "test_stylus";

  // Add an external stylus to our test device manager.
  ui::TouchscreenDevice stylus(0 /* id */, ui::INPUT_DEVICE_USB,
                               kTestStylusName, gfx::Size(),
                               1 /* touch_points */, true /* has_stylus */);
  stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

  ws::InputDeviceClientTestApi().SetTouchscreenDevices({stylus});

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // Verify that when the battery level is 50, no stylus low battery
  // notification is shown.
  observer_->PeripheralBatteryStatusReceived(kTestStylusBatteryPath,
                                             kTestStylusName, 50);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  PeripheralBatteryNotifier::kStylusNotificationId) == nullptr);

  // Verify that when the battery level is 5, a stylus low battery notification
  // is shown. Also check that a non stylus device low battery notification will
  // not show up.
  observer_->PeripheralBatteryStatusReceived(kTestStylusBatteryPath,
                                             kTestStylusName, 5);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  PeripheralBatteryNotifier::kStylusNotificationId) != nullptr);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  kTestBatteryAddress) == nullptr);

  // Verify that when the battery level is -1, the previous stylus low battery
  // notification is cancelled.
  observer_->PeripheralBatteryStatusReceived(kTestStylusBatteryPath,
                                             kTestStylusName, -1);
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
                  PeripheralBatteryNotifier::kStylusNotificationId) == nullptr);
}

}  // namespace ash
