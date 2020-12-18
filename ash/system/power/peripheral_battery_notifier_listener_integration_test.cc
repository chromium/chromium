// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/peripheral_battery_listener.h"

#include <memory>

#include "ash/shell.h"
#include "ash/system/power/peripheral_battery_notifier.h"
#include "ash/system/power/peripheral_battery_tests.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_tick_clock.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using testing::NiceMock;

namespace {

const base::string16& NotificationMessagePrefix() {
  static const base::string16 prefix(base::ASCIIToUTF16("Battery low ("));
  return prefix;
}

const base::string16& NotificationMessageSuffix() {
  static const base::string16 suffix(base::ASCIIToUTF16("%)"));
  return suffix;
}

}  // namespace

namespace ash {

class PeripheralBatteryNotifierListenerTest : public AshTestBase {
 public:
  PeripheralBatteryNotifierListenerTest() = default;
  PeripheralBatteryNotifierListenerTest(
      const PeripheralBatteryNotifierListenerTest&) = delete;
  PeripheralBatteryNotifierListenerTest& operator=(
      const PeripheralBatteryNotifierListenerTest&) = delete;
  ~PeripheralBatteryNotifierListenerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    mock_device_1_ = std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        mock_adapter_.get(), /*bluetooth_class=*/0, kBluetoothDeviceName1,
        kBluetoothDeviceAddress1, /*paired=*/true, /*connected=*/true);
    mock_device_2_ = std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        mock_adapter_.get(), /*bluetooth_class=*/0, kBluetoothDeviceName2,
        kBluetoothDeviceAddress2, /*paired=*/true, /*connected=*/true);

    message_center_ = message_center::MessageCenter::Get();

    battery_listener_ = std::make_unique<PeripheralBatteryListener>();
    battery_notifier_ =
        std::make_unique<PeripheralBatteryNotifier>(battery_listener_.get());
    // No notifications should have been posted yet.
    ASSERT_EQ(0u, message_center_->NotificationCount());
  }

  void TearDown() override {
    battery_notifier_.reset();
    battery_listener_.reset();
    AshTestBase::TearDown();
  }

  // Extracts the battery percentage from the message of a notification.
  uint8_t ExtractBatteryPercentage(message_center::Notification* notification) {
    const base::string16& message = notification->message();
    EXPECT_TRUE(base::StartsWith(message, NotificationMessagePrefix(),
                                 base::CompareCase::SENSITIVE));
    EXPECT_TRUE(base::EndsWith(message, NotificationMessageSuffix(),
                               base::CompareCase::SENSITIVE));

    int prefix_size = NotificationMessagePrefix().size();
    int suffix_size = NotificationMessageSuffix().size();
    int key_len = message.size() - prefix_size - suffix_size;
    EXPECT_GT(key_len, 0);

    int battery_percentage;
    EXPECT_TRUE(base::StringToInt(message.substr(prefix_size, key_len),
                                  &battery_percentage));
    EXPECT_GE(battery_percentage, 0);
    EXPECT_LE(battery_percentage, 100);
    return battery_percentage;
  }

  void SetTestingClock(base::SimpleTestTickClock* clock) {
    battery_notifier_->clock_ = clock;
    battery_listener_->clock_ = clock;
  }

  base::TimeTicks GetTestingClock() {
    // TODO(crbug/1153985): the next line should use clock_->NowTicks()
    return base::TimeTicks();
  }

 protected:
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_1_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_2_;
  message_center::MessageCenter* message_center_;
  std::unique_ptr<PeripheralBatteryNotifier> battery_notifier_;
  std::unique_ptr<PeripheralBatteryListener> battery_listener_;
};

TEST_F(PeripheralBatteryNotifierListenerTest, Basic) {
  base::SimpleTestTickClock clock;
  SetTestingClock(&clock);

  // Level 50 at time 100, no low-battery notification.
  clock.Advance(base::TimeDelta::FromSeconds(100));
  battery_listener_->PeripheralBatteryStatusReceived(kTestBatteryPath,
                                                     kTestDeviceName, 50);
  EXPECT_EQ(1u,
            battery_notifier_->battery_notifications_.count(kTestBatteryId));

  const PeripheralBatteryNotifier::NotificationInfo& info =
      battery_notifier_->battery_notifications_[kTestBatteryId];

  EXPECT_EQ(base::nullopt, info.level);
  EXPECT_EQ(GetTestingClock(), info.last_notification_timestamp);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  // Level 5 at time 110, low-battery notification.
  clock.Advance(base::TimeDelta::FromSeconds(10));
  battery_listener_->PeripheralBatteryStatusReceived(kTestBatteryPath,
                                                     kTestDeviceName, 5);
  EXPECT_EQ(5, info.level);

  // TODO(crbug/1153985): the next line should use GetTestingClock()
  EXPECT_EQ(clock.NowTicks(), info.last_notification_timestamp);
  EXPECT_TRUE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  // Verify that the low-battery notification for stylus does not show up.
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));

  // Level -1 at time 115, cancel previous notification
  clock.Advance(base::TimeDelta::FromSeconds(5));
  battery_listener_->PeripheralBatteryStatusReceived(kTestBatteryPath,
                                                     kTestDeviceName, -1);
  EXPECT_EQ(base::nullopt, info.level);
  // TODO(crbug/1153985): the next line should use GetTestingClock()
  EXPECT_EQ(clock.NowTicks() - base::TimeDelta::FromSeconds(5),
            info.last_notification_timestamp);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  // Level 50 at time 120, no low-battery notification.
  clock.Advance(base::TimeDelta::FromSeconds(5));
  battery_listener_->PeripheralBatteryStatusReceived(kTestBatteryPath,
                                                     kTestDeviceName, 50);
  EXPECT_EQ(base::nullopt, info.level);
  // TODO(crbug/1153985): the next line should use GetTestingClock()
  EXPECT_EQ(clock.NowTicks() - base::TimeDelta::FromSeconds(10),
            info.last_notification_timestamp);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  // Level 5 at time 130, no low-battery notification (throttling).
  clock.Advance(base::TimeDelta::FromSeconds(10));
  battery_listener_->PeripheralBatteryStatusReceived(kTestBatteryPath,
                                                     kTestDeviceName, 5);
  EXPECT_EQ(5, info.level);
  // TODO(crbug/1153985): the next line should use GetTestingClock()
  EXPECT_EQ(clock.NowTicks() - base::TimeDelta::FromSeconds(20),
            info.last_notification_timestamp);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));
}

TEST_F(PeripheralBatteryNotifierListenerTest, InvalidBatteryInfo) {
  const std::string invalid_path1 = "invalid-path";
  const std::string invalid_path2 = "/sys/class/power_supply/hid-battery";

  battery_listener_->PeripheralBatteryStatusReceived(invalid_path1,
                                                     kTestDeviceName, 10);
  EXPECT_TRUE(battery_notifier_->battery_notifications_.empty());

  battery_listener_->PeripheralBatteryStatusReceived(invalid_path2,
                                                     kTestDeviceName, 10);
  EXPECT_TRUE(battery_notifier_->battery_notifications_.empty());

  battery_listener_->PeripheralBatteryStatusReceived(kTestBatteryPath,
                                                     kTestDeviceName, -2);
  EXPECT_TRUE(battery_notifier_->battery_notifications_.empty());

  battery_listener_->PeripheralBatteryStatusReceived(kTestBatteryPath,
                                                     kTestDeviceName, 101);
  EXPECT_TRUE(battery_notifier_->battery_notifications_.empty());

  battery_listener_->PeripheralBatteryStatusReceived(kTestBatteryPath,
                                                     kTestDeviceName, -1);
  EXPECT_TRUE(battery_notifier_->battery_notifications_.empty());
}

// Verify that for Bluetooth devices, the correct address gets stored in the
// BatteryInfo's bluetooth_address member, and for non-Bluetooth devices, that
// bluetooth_address member is empty.
TEST_F(PeripheralBatteryNotifierListenerTest, ExtractBluetoothAddress) {
  const std::string bluetooth_path =
      "/sys/class/power_supply/hid-A0:b1:C2:d3:E4:f5-battery";
  const std::string expected_bluetooth_address = "a0:b1:c2:d3:e4:f5";
  const std::string expected_bluetooth_id =
      "battery_notification_bluetooth-a0:b1:c2:d3:e4:f5";
  const std::string non_bluetooth_path =
      "/sys/class/power_supply/hid-notbluetooth-battery";

  battery_listener_->PeripheralBatteryStatusReceived(bluetooth_path,
                                                     kTestDeviceName, 10);
  battery_listener_->PeripheralBatteryStatusReceived(non_bluetooth_path,
                                                     kTestDeviceName, 10);
  EXPECT_EQ(2u, battery_notifier_->battery_notifications_.size());
}

TEST_F(PeripheralBatteryNotifierListenerTest, DeviceRemove) {
  battery_listener_->PeripheralBatteryStatusReceived(kTestBatteryPath,
                                                     kTestDeviceName, 5);
  EXPECT_EQ(1u,
            battery_notifier_->battery_notifications_.count(kTestBatteryId));
  EXPECT_TRUE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));
}

TEST_F(PeripheralBatteryNotifierListenerTest, StylusNotification) {
  const std::string kTestStylusBatteryPath =
      "/sys/class/power_supply/hid-AAAA:BBBB:CCCC.DDDD-battery";
  const std::string kTestStylusName = "test_stylus";

  // Add an external stylus to our test device manager.
  ui::TouchscreenDevice stylus(/*id=*/0, ui::INPUT_DEVICE_USB, kTestStylusName,
                               gfx::Size(),
                               /*touch_points=*/1, /*has_stylus=*/true);
  stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({stylus});

  // Verify that when the battery level is 50, no stylus low battery
  // notification is shown.
  battery_listener_->PeripheralBatteryStatusReceived(kTestStylusBatteryPath,
                                                     kTestStylusName, 50);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));

  // Verify that when the battery level is 5, a stylus low battery notification
  // is shown. Also check that a non stylus device low battery notification will
  // not show up.
  battery_listener_->PeripheralBatteryStatusReceived(kTestStylusBatteryPath,
                                                     kTestStylusName, 5);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryAddress));

  // Verify that when the battery level is -1, the previous stylus low battery
  // notification is cancelled.
  battery_listener_->PeripheralBatteryStatusReceived(kTestStylusBatteryPath,
                                                     kTestStylusName, -1);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));
}

TEST_F(PeripheralBatteryNotifierListenerTest,
       Bluetooth_OnlyShowNotificationForLowBatteryLevels) {
  // Should not create a notification for battery changes above the threshold.
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/80);
  EXPECT_EQ(0u, message_center_->NotificationCount());
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/100);
  EXPECT_EQ(0u, message_center_->NotificationCount());

  // Should trigger notificaiton.
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/10);
  EXPECT_EQ(1u, message_center_->NotificationCount());
  message_center::Notification* notification =
      message_center_->FindVisibleNotificationById(
          kBluetoothDeviceNotificationId1);
  EXPECT_EQ(mock_device_1_->GetNameForDisplay(), notification->title());
  EXPECT_EQ(10, ExtractBatteryPercentage(notification));
}

TEST_F(PeripheralBatteryNotifierListenerTest,
       Bluetooth_CreatesANotificationForEachDevice) {
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/5);
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_2_.get(),
                                          /*new_battery_percentage=*/0);

  // Verify 2 notifications were posted with the correct values.
  EXPECT_EQ(2u, message_center_->NotificationCount());
  message_center::Notification* notification_1 =
      message_center_->FindVisibleNotificationById(
          kBluetoothDeviceNotificationId1);
  message_center::Notification* notification_2 =
      message_center_->FindVisibleNotificationById(
          kBluetoothDeviceNotificationId2);

  EXPECT_TRUE(notification_1);
  EXPECT_EQ(mock_device_1_->GetNameForDisplay(), notification_1->title());
  EXPECT_EQ(5, ExtractBatteryPercentage(notification_1));
  EXPECT_TRUE(notification_2);
  EXPECT_EQ(mock_device_2_->GetNameForDisplay(), notification_2->title());
  EXPECT_EQ(0, ExtractBatteryPercentage(notification_2));
}

TEST_F(PeripheralBatteryNotifierListenerTest,
       Bluetooth_RemovesNotificationForDisconnectedDevices) {
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/5);
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_2_.get(),
                                          /*new_battery_percentage=*/0);

  // Verify 2 notifications were posted.
  EXPECT_EQ(2u, message_center_->NotificationCount());

  // Verify only the notification for device 1 gets removed.
  battery_listener_->DeviceConnectedStateChanged(mock_adapter_.get(),
                                                 mock_device_1_.get(), false);
  EXPECT_EQ(1u, message_center_->NotificationCount());
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId2));

  // Remove the second notification.
  battery_listener_->DeviceRemoved(mock_adapter_.get(), mock_device_2_.get());
  EXPECT_EQ(0u, message_center_->NotificationCount());
}

TEST_F(PeripheralBatteryNotifierListenerTest,
       Bluetooth_CancelNotificationForInvalidBatteryLevel) {
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/1);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // The notification should get canceled.
  battery_listener_->DeviceBatteryChanged(
      mock_adapter_.get(), mock_device_1_.get(),
      /*new_battery_percentage=*/base::nullopt);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));
}

// Don't post a notification if the battery level drops again under the
// threshold before kNotificationInterval is completed.
TEST_F(PeripheralBatteryNotifierListenerTest,
       DontShowSecondNotificationWithinASmallTimeInterval) {
  base::SimpleTestTickClock clock;
  SetTestingClock(&clock);
  clock.Advance(base::TimeDelta::FromSeconds(100));

  // Post a notification.
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/1);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // Cancel the notification.
  clock.Advance(base::TimeDelta::FromSeconds(1));
  battery_listener_->DeviceBatteryChanged(
      mock_adapter_.get(), mock_device_1_.get(),
      /*new_battery_percentage=*/base::nullopt);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // The battery level falls below the threshold after a short time period. No
  // notification should get posted.
  clock.Advance(base::TimeDelta::FromSeconds(1));
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/1);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));
}

// Post a notification if the battery is under threshold, then unknown level and
// then is again under the threshold after kNotificationInterval is completed.
TEST_F(PeripheralBatteryNotifierListenerTest,
       PostNotificationIfBatteryGoesFromUnknownLevelToBelowThreshold) {
  base::SimpleTestTickClock clock;
  SetTestingClock(&clock);
  clock.Advance(base::TimeDelta::FromSeconds(100));

  // Post a notification.
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/1);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // Cancel the notification.
  clock.Advance(base::TimeDelta::FromSeconds(1));
  battery_listener_->DeviceBatteryChanged(
      mock_adapter_.get(), mock_device_1_.get(),
      /*new_battery_percentage=*/base::nullopt);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // Post notification if we are out of the kNotificationInterval.
  clock.Advance(base::TimeDelta::FromSeconds(100));
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/1);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));
}

// Don't Post another notification if the battery level keeps low and the user
// dismissed the previous notification.
TEST_F(PeripheralBatteryNotifierListenerTest,
       DontRepostNotificationIfUserDismissedPreviousOne) {
  base::SimpleTestTickClock clock;
  SetTestingClock(&clock);
  clock.Advance(base::TimeDelta::FromSeconds(100));

  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/5);
  EXPECT_EQ(1u, message_center_->NotificationCount());

  // Simulate the user clears the notification.
  message_center_->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  // The battery level remains low, but shouldn't post a notificaiton.
  clock.Advance(base::TimeDelta::FromSeconds(100));
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/3);
  EXPECT_EQ(0u, message_center_->NotificationCount());
}

// If there is an existing notificaiton and the battery level remains low,
// update its content.
TEST_F(PeripheralBatteryNotifierListenerTest, UpdateNotificationIfVisible) {
  base::SimpleTestTickClock clock;
  SetTestingClock(&clock);
  clock.Advance(base::TimeDelta::FromSeconds(100));

  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/5);
  EXPECT_EQ(1u, message_center_->NotificationCount());

  // The battery level remains low, should update the notification.
  clock.Advance(base::TimeDelta::FromSeconds(100));
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/3);

  message_center::Notification* notification =
      message_center_->FindVisibleNotificationById(
          kBluetoothDeviceNotificationId1);
  EXPECT_TRUE(notification);
  EXPECT_EQ(mock_device_1_->GetNameForDisplay(), notification->title());
  EXPECT_EQ(3, ExtractBatteryPercentage(notification));
}

}  // namespace ash
