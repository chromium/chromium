// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/power/peripheral_battery_listener.h"
#include "ash/system/power/peripheral_battery_notifier.h"
#include "ash/system/power/peripheral_battery_tests.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using testing::NiceMock;
using BatteryInfo = device::BluetoothDevice::BatteryInfo;
using BatteryType = device::BluetoothDevice::BatteryType;

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

class PeripheralBatteryNotifierListenerTest : public AshTestBase {
 public:
  // Constants for active field of PeripheralBatteryStylusReceived().
  const bool kBluetoothBatteryUpdate = true;
  const bool kBatteryPolledUpdate = false;
  const bool kBatteryEventUpdate = true;

  PeripheralBatteryNotifierListenerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  PeripheralBatteryNotifierListenerTest(
      const PeripheralBatteryNotifierListenerTest&) = delete;
  PeripheralBatteryNotifierListenerTest& operator=(
      const PeripheralBatteryNotifierListenerTest&) = delete;
  ~PeripheralBatteryNotifierListenerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    ASSERT_TRUE(ui::DeviceDataManager::HasInstance());

    // Simulate the complete listing of input devices, required by the listener.
    if (complete_devices_)
      ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    mock_device_1_ = std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        mock_adapter_.get(), /*bluetooth_class=*/0, kBluetoothDeviceName1,
        kBluetoothDeviceAddress1, /*paired=*/true, /*connected=*/true);
    mock_device_2_ = std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        mock_adapter_.get(), /*bluetooth_class=*/0, kBluetoothDeviceName2,
        kBluetoothDeviceAddress2, /*paired=*/true, /*connected=*/true);

    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

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

  void SendBatteryUpdate(const std::string& path,
                         const std::string& name,
                         int level) {
    battery_listener_->PeripheralBatteryStatusReceived(
        path, name, level,
        power_manager::
            PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING,
        /*serial_number=*/kStylusEligibleSerialNumbers[0],
        kBatteryPolledUpdate);
  }

  void SendBatteryUpdate(
      const std::string& path,
      const std::string& name,
      int level,
      power_manager::PeripheralBatteryStatus_ChargeStatus status,
      const std::string& serial_number,
      bool active_update) {
    battery_listener_->PeripheralBatteryStatusReceived(
        path, name, level, status, serial_number, active_update);
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

 protected:
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_1_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_2_;
  raw_ptr<message_center::MessageCenter, DanglingUntriaged> message_center_;
  std::unique_ptr<PeripheralBatteryNotifier> battery_notifier_;
  std::unique_ptr<PeripheralBatteryListener> battery_listener_;

  void set_complete_devices(bool complete_devices) {
    complete_devices_ = complete_devices;
  }

  // SetUp() doesn't complete devices if this is set to false.
  bool complete_devices_ = true;
};

class PeripheralBatteryNotifierListenerIncompleteDevicesTest
    : public PeripheralBatteryNotifierListenerTest {
 public:
  PeripheralBatteryNotifierListenerIncompleteDevicesTest() {
    set_complete_devices(false);
  }

  PeripheralBatteryNotifierListenerIncompleteDevicesTest(
      const PeripheralBatteryNotifierListenerIncompleteDevicesTest&) = delete;
  PeripheralBatteryNotifierListenerIncompleteDevicesTest& operator=(
      const PeripheralBatteryNotifierListenerIncompleteDevicesTest&) = delete;

  ~PeripheralBatteryNotifierListenerIncompleteDevicesTest() override {}
};

TEST_F(PeripheralBatteryNotifierListenerTest, Basic) {
  // Level 50 at time 100, no low-battery notification.
  ClockAdvance(base::Seconds(100));
  SendBatteryUpdate(kTestBatteryPath, kTestDeviceName, 50);
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
  SendBatteryUpdate(
      kTestBatteryPath, kTestDeviceName, 5,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING,
      /*serial_number=*/"", kBatteryEventUpdate);
  EXPECT_EQ(5, info.level);

  EXPECT_EQ(GetTestingClock(), info.last_notification_timestamp);
  EXPECT_TRUE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  // Verify that the low-battery notification for stylus does not show up.
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));

  // Level -1 at time 115, cancel previous notification
  ClockAdvance(base::Seconds(5));
  SendBatteryUpdate(kTestBatteryPath, kTestDeviceName, -1);
  EXPECT_EQ(std::nullopt, info.level);
  EXPECT_EQ(GetTestingClock() - base::Seconds(5),
            info.last_notification_timestamp);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  // Level 50 at time 120, no low-battery notification.
  ClockAdvance(base::Seconds(5));
  SendBatteryUpdate(kTestBatteryPath, kTestDeviceName, 50);
  EXPECT_EQ(std::nullopt, info.level);
  EXPECT_EQ(GetTestingClock() - base::Seconds(10),
            info.last_notification_timestamp);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  // Level 5 at time 130, no low-battery notification (throttling).
  ClockAdvance(base::Seconds(10));
  SendBatteryUpdate(kTestBatteryPath, kTestDeviceName, 5);
  EXPECT_EQ(5, info.level);
  EXPECT_EQ(GetTestingClock() - base::Seconds(20),
            info.last_notification_timestamp);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));
}

TEST_F(PeripheralBatteryNotifierListenerTest, InvalidBatteryInfo) {
  const std::string invalid_path1 = "invalid-path";
  const std::string invalid_path2 = "/sys/class/power_supply/hid-battery";

  SendBatteryUpdate(invalid_path1, kTestDeviceName, 10);
  EXPECT_TRUE(battery_notifier_->battery_notifications_.empty());

  SendBatteryUpdate(invalid_path2, kTestDeviceName, 10);
  EXPECT_TRUE(battery_notifier_->battery_notifications_.empty());

  SendBatteryUpdate(kTestBatteryPath, kTestDeviceName, -2);
  EXPECT_TRUE(battery_notifier_->battery_notifications_.empty());

  SendBatteryUpdate(kTestBatteryPath, kTestDeviceName, 101);
  EXPECT_TRUE(battery_notifier_->battery_notifications_.empty());

  SendBatteryUpdate(kTestBatteryPath, kTestDeviceName, -1);
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

  SendBatteryUpdate(
      bluetooth_path, kTestDeviceName, 10,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING,
      /*serial_number=*/"", kBluetoothBatteryUpdate);
  SendBatteryUpdate(
      non_bluetooth_path, kTestDeviceName, 10,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING,
      /*serial_number=*/"", kBatteryPolledUpdate);
  EXPECT_EQ(2u, battery_notifier_->battery_notifications_.size());
}

TEST_F(PeripheralBatteryNotifierListenerTest, DeviceRemove) {
  SendBatteryUpdate(kTestBatteryPath, kTestDeviceName, 5);
  EXPECT_EQ(1u,
            battery_notifier_->battery_notifications_.count(kTestBatteryId));
  EXPECT_TRUE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));

  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryNotificationId));
}

TEST_F(PeripheralBatteryNotifierListenerIncompleteDevicesTest,
       StylusNotification) {
  const std::string kTestStylusBatteryPath =
      "/sys/class/power_supply/hid-AAAA:BBBB:CCCC.DDDD-battery";
  const std::string kTestStylusName = "test_stylus";

  // Add an external stylus to our test device manager.
  ui::TouchscreenDevice stylus(/*id=*/0, ui::INPUT_DEVICE_USB, kTestStylusName,
                               gfx::Size(),
                               /*touch_points=*/1, /*has_stylus=*/true);
  stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({stylus});

  // Verify that when the battery level is 5, a stylus low battery notification
  // is not shown due to input device list not being complete. Also check that
  // a non stylus device low battery notification will not show up.
  SendBatteryUpdate(
      kTestStylusBatteryPath, kTestStylusName, 5,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING,
      kStylusEligibleSerialNumbers[0], kBatteryEventUpdate);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryAddress));

  // Complete devices
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  // Verify that when the battery level is 5, a stylus low battery notification
  // is now shown. Also check that a non stylus device low battery notification
  // will still not show up.
  SendBatteryUpdate(
      kTestStylusBatteryPath, kTestStylusName, 5,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING,
      kStylusEligibleSerialNumbers[0], kBatteryEventUpdate);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryAddress));
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
  SendBatteryUpdate(kTestStylusBatteryPath, kTestStylusName, 50);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));

  // Verify that when the battery level is 5, a stylus low battery notification
  // is shown. Also check that a non stylus device low battery notification will
  // not show up.
  SendBatteryUpdate(
      kTestStylusBatteryPath, kTestStylusName, 5,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING,
      kStylusEligibleSerialNumbers[0], kBatteryPolledUpdate);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kTestBatteryAddress));

  // Verify that when the battery level is -1, the previous stylus low battery
  // notification is cancelled.
  SendBatteryUpdate(
      kTestStylusBatteryPath, kTestStylusName, -1,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING,
      kStylusEligibleSerialNumbers[0], kBatteryPolledUpdate);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      PeripheralBatteryNotifier::kStylusNotificationId));
}

TEST_F(PeripheralBatteryNotifierListenerTest,
       Bluetooth_OnlyShowNotificationForLowBatteryLevels) {
  // Should not create a notification for battery changes above the threshold.
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/80));
  EXPECT_EQ(0u, message_center_->NotificationCount());
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/100));
  EXPECT_EQ(0u, message_center_->NotificationCount());

  // Should trigger notification.
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/10));
  EXPECT_EQ(1u, message_center_->NotificationCount());
  message_center::Notification* notification =
      message_center_->FindVisibleNotificationById(
          kBluetoothDeviceNotificationId1);
  EXPECT_EQ(mock_device_1_->GetNameForDisplay(), notification->title());
  EXPECT_EQ(10, ExtractBatteryPercentage(notification));
}

TEST_F(PeripheralBatteryNotifierListenerTest,
       Bluetooth_CreatesANotificationForEachDevice) {
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/5));
  mock_device_2_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/0));

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
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/5));
  mock_device_2_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/0));

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
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/1));
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // The notification should get canceled.
  mock_device_1_->RemoveBatteryInfo(BatteryType::kDefault);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));
}

// Don't post a notification if the battery level drops again under the
// threshold before kNotificationInterval is completed.
TEST_F(PeripheralBatteryNotifierListenerTest,
       DontShowSecondNotificationWithinASmallTimeInterval) {
  ClockAdvance(base::Seconds(100));

  // Post a notification.
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/1));
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // Cancel the notification.
  ClockAdvance(base::Seconds(1));
  mock_device_1_->RemoveBatteryInfo(BatteryType::kDefault);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // The battery level falls below the threshold after a short time period. No
  // notification should get posted.
  ClockAdvance(base::Seconds(1));
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/1));
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));
}

// Post a notification if the battery is under threshold, then unknown level and
// then is again under the threshold after kNotificationInterval is completed.
TEST_F(PeripheralBatteryNotifierListenerTest,
       PostNotificationIfBatteryGoesFromUnknownLevelToBelowThreshold) {
  ClockAdvance(base::Seconds(100));

  // Post a notification.
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/1));
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // Cancel the notification.
  ClockAdvance(base::Seconds(1));
  mock_device_1_->RemoveBatteryInfo(BatteryType::kDefault);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));

  // Post notification if we are out of the kNotificationInterval.
  ClockAdvance(base::Seconds(100));
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/1));
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kBluetoothDeviceNotificationId1));
}

// Don't Post another notification if the battery level keeps low and the user
// dismissed the previous notification.
TEST_F(PeripheralBatteryNotifierListenerTest,
       DontRepostNotificationIfUserDismissedPreviousOne) {
  ClockAdvance(base::Seconds(100));

  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/5));
  EXPECT_EQ(1u, message_center_->NotificationCount());

  // Simulate the user clears the notification.
  message_center_->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  // The battery level remains low, but shouldn't post a notification.
  ClockAdvance(base::Seconds(100));
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/3));
  EXPECT_EQ(0u, message_center_->NotificationCount());
}

// If there is an existing notification and the battery level remains low,
// update its content.
TEST_F(PeripheralBatteryNotifierListenerTest, UpdateNotificationIfVisible) {
  ClockAdvance(base::Seconds(100));

  mock_device_1_->SetBatteryInfo(BatteryInfo(BatteryType::kDefault, 5));
  EXPECT_EQ(1u, message_center_->NotificationCount());

  // The battery level remains low, should update the notification.
  ClockAdvance(base::Seconds(100));
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/3));

  message_center::Notification* notification =
      message_center_->FindVisibleNotificationById(
          kBluetoothDeviceNotificationId1);
  EXPECT_TRUE(notification);
  EXPECT_EQ(mock_device_1_->GetNameForDisplay(), notification->title());
  EXPECT_EQ(3, ExtractBatteryPercentage(notification));
}

}  // namespace ash
