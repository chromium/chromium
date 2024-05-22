// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/peripheral_battery_notifier.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/system/power/peripheral_battery_listener.h"
#include "ash/system/power/peripheral_battery_tests.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using BI = ash::PeripheralBatteryListener::BatteryInfo;

namespace {

const std::u16string& NotificationMessagePrefix() {
  static const std::u16string prefix(u"Battery low (");
  return prefix;
}

const std::u16string& NotificationMessageSuffix() {
  static const std::u16string suffix(u"%)");
  return suffix;
}

}  // namespace

namespace ash {

class PeripheralBatteryNotifierTest : public AshTestBase {
 public:
  PeripheralBatteryNotifierTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  PeripheralBatteryNotifierTest(const PeripheralBatteryNotifierTest&) = delete;
  PeripheralBatteryNotifierTest& operator=(
      const PeripheralBatteryNotifierTest&) = delete;
  ~PeripheralBatteryNotifierTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    AshTestBase::SetUp();
    ASSERT_TRUE(ui::DeviceDataManager::HasInstance());

    // Simulate the complete listing of input devices, required by the listener.
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    message_center_ = message_center::MessageCenter::Get();
    system_tray_client_ = GetSystemTrayClient();

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
    chromeos::PowerManagerClient::Shutdown();
  }

  // Extracts the battery percentage from the message of a notification.
  uint8_t ExtractBatteryPercentage(message_center::Notification* notification) {
    const std::u16string& message = notification->message();
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

  base::TimeTicks GetTestingClock() { return base::TimeTicks::Now(); }

  void ClockAdvance(base::TimeDelta delta) {
    task_environment()->AdvanceClock(delta);
  }

  void UpdateBatteryLevel(bool add_first,
                          const std::string key,
                          const std::u16string name,
                          std::optional<uint8_t> level,
                          bool battery_report_eligible,
                          BI::PeripheralType type,
                          const std::string btaddr) {
    BI info(key, name, level, battery_report_eligible, GetTestingClock(), type,
            BI::ChargeStatus::kUnknown, btaddr);
    if (add_first)
      battery_notifier_->OnAddingBattery(info);
    battery_notifier_->OnUpdatedBatteryLevel(info);
  }

  void RemoveBattery(const std::string key,
                     const std::u16string name,
                     std::optional<uint8_t> level,
                     bool battery_report_eligible,
                     BI::PeripheralType type,
                     const std::string btaddr) {
    BI info(key, name, level, battery_report_eligible, GetTestingClock(), type,
            BI::ChargeStatus::kUnknown, btaddr);
    battery_notifier_->OnRemovingBattery(info);
  }

 protected:
  raw_ptr<message_center::MessageCenter, DanglingUntriaged> message_center_;
  raw_ptr<TestSystemTrayClient, DanglingUntriaged> system_tray_client_;
  std::unique_ptr<PeripheralBatteryNotifier> battery_notifier_;
  std::unique_ptr<PeripheralBatteryListener> battery_listener_;

  void set_complete_devices(bool complete_devices) {
    complete_devices_ = complete_devices;
  }

  // SetUp() doesn't complete devices if this is set to false.
  bool complete_devices_ = true;
};

TEST_F(PeripheralBatteryNotifierTest, Basic) {

  // Level 50 at time 100, no low-battery notification.
  ClockAdvance(base::Seconds(100));
  UpdateBatteryLevel(true, kTestBatteryId, kTestDeviceName16, 50,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kTestBatteryAddress);
  EXPECT_EQ(1u,
            battery_notifier_->battery_notifications_.count(kTestBatteryId));

  const PeripheralBatteryNotifier::NotificationInfo& info =
      battery_notifier_->battery_notifications_[kTestBatteryId];

  EXPECT_EQ(std::nullopt, info.level);
  EXPECT_EQ(GetTestingClock(), info.last_notification_timestamp);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  // Level 5 at time 110, low-battery notification.
  ClockAdvance(base::Seconds(10));
  UpdateBatteryLevel(false, kTestBatteryId, kTestDeviceName16, 5,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kTestBatteryAddress);
  EXPECT_EQ(5, info.level);

  EXPECT_EQ(GetTestingClock(), info.last_notification_timestamp);
  EXPECT_TRUE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  // Verify that the low-battery notification for stylus does not show up.
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));

  // Level -1 at time 115, cancel previous notification.
  ClockAdvance(base::Seconds(5));
  UpdateBatteryLevel(false, kTestBatteryId, kTestDeviceName16, std::nullopt,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kTestBatteryAddress);
  EXPECT_EQ(std::nullopt, info.level);
  EXPECT_EQ(GetTestingClock() - base::Seconds(5),
            info.last_notification_timestamp);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  // Level 50 at time 120, no low-battery notification.
  ClockAdvance(base::Seconds(5));
  UpdateBatteryLevel(false, kTestBatteryId, kTestDeviceName16, 50,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kTestBatteryAddress);
  EXPECT_EQ(std::nullopt, info.level);
  EXPECT_EQ(GetTestingClock() - base::Seconds(10),
            info.last_notification_timestamp);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  // Level 5 at time 130, no low-battery notification (throttling).
  ClockAdvance(base::Seconds(10));
  UpdateBatteryLevel(false, kTestBatteryId, kTestDeviceName16, 5,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kTestBatteryAddress);
  EXPECT_EQ(5, info.level);
  EXPECT_EQ(GetTestingClock() - base::Seconds(20),
            info.last_notification_timestamp);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));
}

TEST_F(PeripheralBatteryNotifierTest, EarlyNotification) {
  // Level 15 at time 10, low-battery notification.
  ClockAdvance(base::Seconds(10));
  UpdateBatteryLevel(true, kTestBatteryId, kTestDeviceName16, 15,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kTestBatteryAddress);
  EXPECT_EQ(1u,
            battery_notifier_->battery_notifications_.count(kTestBatteryId));

  const PeripheralBatteryNotifier::NotificationInfo& info =
      battery_notifier_->battery_notifications_[kTestBatteryId];

  EXPECT_EQ(15, info.level);
  EXPECT_EQ(GetTestingClock(), info.last_notification_timestamp);
  EXPECT_TRUE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));
}

TEST_F(PeripheralBatteryNotifierTest, StylusNotification) {
  const std::string kTestStylusBatteryPath =
      "/sys/class/power_supply/hid-AAAA:BBBB:CCCC.DDDD-battery";
  const std::string kTestStylusBatteryId =
      "???hxxxxid-AAAA:BBBB:CCCC.DDDD-battery";
  const std::string kTestStylusName = "test_stylus";
  const std::u16string kTestStylusName16 = u"test_stylus";

  // Add an external stylus to our test device manager.
  ui::TouchscreenDevice stylus(
      /*id=*/0, ui::INPUT_DEVICE_USB, kTestStylusName, gfx::Size(),
      /*touch_points=*/1,
      /*has_stylus=*/true);
  stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({stylus});

  // Verify that when the battery level is 50, no stylus low battery
  // notification is shown.
  UpdateBatteryLevel(true, kTestStylusBatteryId, kTestStylusName16, 50,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kStylusViaScreen, "");
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));

  // Verify that when the battery level is 5, a stylus low battery notification
  // is shown. Also check that a non stylus device low battery notification will
  // not show up.
  UpdateBatteryLevel(false, kTestStylusBatteryId, kTestStylusName16, 5,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kStylusViaScreen, "");
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryAddress));

  // Verify that when the battery level is -1, the previous stylus low battery
  // notification is cancelled.
  UpdateBatteryLevel(false, kTestStylusBatteryId, kTestStylusName16,
                     std::nullopt, /*battery_report_eligible=*/true,
                     BI::PeripheralType::kStylusViaScreen, "");
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));
}

TEST_F(PeripheralBatteryNotifierTest,
       Bluetooth_CreatesANotificationForEachDevice) {
  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116, 5,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  UpdateBatteryLevel(true, kBluetoothDeviceId2, kBluetoothDeviceName216, 0,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress2);

  // Verify 2 notifications were posted with the correct values.
  EXPECT_EQ(2u, message_center_->NotificationCount());
  message_center::Notification* notification_1 =
      message_center_->FindVisibleNotificationById(
          kBluetoothDeviceNotificationId1);
  message_center::Notification* notification_2 =
      message_center_->FindVisibleNotificationById(
          kBluetoothDeviceNotificationId2);

  EXPECT_TRUE(notification_1);
  EXPECT_EQ(kBluetoothDeviceName116, notification_1->title());
  EXPECT_EQ(5, ExtractBatteryPercentage(notification_1));
  EXPECT_TRUE(notification_2);
  EXPECT_EQ(kBluetoothDeviceName216, notification_2->title());
  EXPECT_EQ(0, ExtractBatteryPercentage(notification_2));
}

TEST_F(PeripheralBatteryNotifierTest,
       Bluetooth_RemovesNotificationForDisconnectedDevices) {
  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116, 5,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  UpdateBatteryLevel(true, kBluetoothDeviceId2, kBluetoothDeviceName216, 0,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress2);

  // Verify 2 notifications were posted.
  EXPECT_EQ(2u, message_center_->NotificationCount());

  // Verify only the notification for device 1 gets removed.
  RemoveBattery(kBluetoothDeviceId1, kBluetoothDeviceName116, 5,
                /*battery_report_eligible=*/true, BI::PeripheralType::kOther,
                kBluetoothDeviceAddress1);
  EXPECT_EQ(1u, message_center_->NotificationCount());
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId2));

  // Remove the second notification.
  RemoveBattery(kBluetoothDeviceId2, kBluetoothDeviceName216, 0,
                /*battery_report_eligible=*/true, BI::PeripheralType::kOther,
                kBluetoothDeviceAddress2);
  EXPECT_EQ(0u, message_center_->NotificationCount());
}

TEST_F(PeripheralBatteryNotifierTest,
       Bluetooth_CancelNotificationForInvalidBatteryLevel) {
  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116, 1,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // The notification should get canceled.
  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116,
                     std::nullopt, /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));
}

// Don't post a notification if the battery level drops again under the
// threshold before kNotificationInterval is completed.
TEST_F(PeripheralBatteryNotifierTest,
       DontShowSecondNotificationWithinASmallTimeInterval) {
  ClockAdvance(base::Seconds(100));

  // Post a notification.

  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116, 1,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // Cancel the notification.
  ClockAdvance(base::Seconds(1));
  UpdateBatteryLevel(false, kBluetoothDeviceId1, kBluetoothDeviceName116,
                     std::nullopt, /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // The battery level falls below the threshold after a short time period. No
  // notification should get posted.
  ClockAdvance(base::Seconds(1));
  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116, 1,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));
}

// Post a notification if the battery is under threshold, then unknown level and
// then is again under the threshold after kNotificationInterval is completed.
TEST_F(PeripheralBatteryNotifierTest,
       PostNotificationIfBatteryGoesFromUnknownLevelToBelowThreshold) {
  ClockAdvance(base::Seconds(100));

  // Post a notification.
  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116, 1,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // Cancel the notification.
  ClockAdvance(base::Seconds(1));
  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116,
                     std::nullopt, /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // Post notification if we are out of the kNotificationInterval.
  ClockAdvance(base::Seconds(100));
  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116, 1,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));
}

// Don't Post another notification if the battery level keeps low and the user
// dismissed the previous notification.
TEST_F(PeripheralBatteryNotifierTest,
       DontRepostNotificationIfUserDismissedPreviousOne) {
  ClockAdvance(base::Seconds(100));

  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116, 5,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  EXPECT_EQ(1u, message_center_->NotificationCount());

  // Simulate the user clears the notification.
  message_center_->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  // The battery level remains low, but shouldn't post a notification.
  ClockAdvance(base::Seconds(100));
  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116, 5,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  EXPECT_EQ(0u, message_center_->NotificationCount());
}

// If there is an existing notification and the battery level remains low,
// update its content.
TEST_F(PeripheralBatteryNotifierTest, UpdateNotificationIfVisible) {
  ClockAdvance(base::Seconds(100));

  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116, 5,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  EXPECT_EQ(1u, message_center_->NotificationCount());

  // The battery level remains low, should update the notification.
  ClockAdvance(base::Seconds(100));
  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116, 3,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  message_center::Notification* notification =
      message_center_->FindVisibleNotificationById(
          kBluetoothDeviceNotificationId1);
  EXPECT_TRUE(notification);
  EXPECT_EQ(kBluetoothDeviceName116, notification->title());
  EXPECT_EQ(3, ExtractBatteryPercentage(notification));
}

TEST_F(PeripheralBatteryNotifierTest, OpenBluetoothSettingsUi) {
  ClockAdvance(base::Seconds(100));

  UpdateBatteryLevel(true, kBluetoothDeviceId1, kBluetoothDeviceName116, 5,
                     /*battery_report_eligible=*/true,
                     BI::PeripheralType::kOther, kBluetoothDeviceAddress1);
  EXPECT_EQ(1u, message_center_->NotificationCount());

  message_center::Notification* notification =
      message_center_->FindVisibleNotificationById(
          kBluetoothDeviceNotificationId1);
  EXPECT_TRUE(notification);

  message_center_->ClickOnNotification(notification->id());
  EXPECT_EQ(1, system_tray_client_->show_bluetooth_settings_count());
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));
}

}  // namespace ash
