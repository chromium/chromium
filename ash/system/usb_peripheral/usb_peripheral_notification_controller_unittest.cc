// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/usb_peripheral/usb_peripheral_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;

namespace ash {

namespace {

const char kUsbPeripheralInvalidDpCableNotificationId[] =
    "cros_usb_peripheral_invalid_dp_cable_notification_id";
const char kUsbPeripheralInvalidUSB4ValidTBTCableNotificationId[] =
    "cros_usb_peripheral_invalid_usb4_valid_tbt_cable_notification_id";
const char kUsbPeripheralInvalidUSB4CableNotificationId[] =
    "cros_usb_peripheral_invalid_usb4_cable_notification_id";
const char kUsbPeripheralInvalidTBTCableNotificationId[] =
    "cros_usb_peripheral_invalid_tbt_cable_notification_id";
const char kUsbPeripheralSpeedLimitingCableNotificationId[] =
    "cros_usb_peripheral_speed_limiting_cable_notification_id";

}  // namespace

class UsbPeripheralNotificationControllerTest : public AshTestBase {
 public:
  UsbPeripheralNotificationControllerTest() = default;
  UsbPeripheralNotificationControllerTest(
      const UsbPeripheralNotificationControllerTest&) = delete;
  UsbPeripheralNotificationControllerTest& operator=(
      const UsbPeripheralNotificationControllerTest&) = delete;
  ~UsbPeripheralNotificationControllerTest() override = default;

  UsbPeripheralNotificationController* controller() {
    return Shell::Get()->usb_peripheral_notification_controller();
  }

 private:
};

TEST_F(UsbPeripheralNotificationControllerTest, InvalidDpCableNotification) {
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 0u);
  controller()->OnInvalidDpCableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);

  message_center::Notification* notification =
      MessageCenter::Get()->FindVisibleNotificationById(
          kUsbPeripheralInvalidDpCableNotificationId);
  ASSERT_TRUE(notification);

  EXPECT_EQ(notification->buttons().size(), 1u);
  controller()->OnInvalidDpCableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);
}

TEST_F(UsbPeripheralNotificationControllerTest,
       InvalidUSB4ValidTBTCableNotification) {
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 0u);
  controller()->OnInvalidUSB4ValidTBTCableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);

  message_center::Notification* notification =
      MessageCenter::Get()->FindVisibleNotificationById(
          kUsbPeripheralInvalidUSB4ValidTBTCableNotificationId);
  ASSERT_TRUE(notification);

  EXPECT_EQ(notification->buttons().size(), 1u);
  controller()->OnInvalidUSB4ValidTBTCableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);
}

TEST_F(UsbPeripheralNotificationControllerTest, InvalidUSB4CableNotification) {
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 0u);
  controller()->OnInvalidUSB4CableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);

  message_center::Notification* notification =
      MessageCenter::Get()->FindVisibleNotificationById(
          kUsbPeripheralInvalidUSB4CableNotificationId);
  ASSERT_TRUE(notification);

  EXPECT_EQ(notification->buttons().size(), 1u);
  controller()->OnInvalidUSB4CableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);
}

TEST_F(UsbPeripheralNotificationControllerTest, InvalidTBTCableNotification) {
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 0u);
  controller()->OnInvalidTBTCableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);

  message_center::Notification* notification =
      MessageCenter::Get()->FindVisibleNotificationById(
          kUsbPeripheralInvalidTBTCableNotificationId);
  ASSERT_TRUE(notification);

  EXPECT_EQ(notification->buttons().size(), 1u);
  controller()->OnInvalidTBTCableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);
}

TEST_F(UsbPeripheralNotificationControllerTest,
       SpeedLimitingCableNotification) {
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 0u);
  controller()->OnSpeedLimitingCableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);

  message_center::Notification* notification =
      MessageCenter::Get()->FindVisibleNotificationById(
          kUsbPeripheralSpeedLimitingCableNotificationId);
  ASSERT_TRUE(notification);

  EXPECT_EQ(notification->buttons().size(), 1u);
  controller()->OnSpeedLimitingCableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);
}

TEST_F(UsbPeripheralNotificationControllerTest,
       SpeedLimitingCableNotificationWithClick) {
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 0u);
  controller()->OnSpeedLimitingCableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);

  message_center::Notification* notification =
      MessageCenter::Get()->FindVisibleNotificationById(
          kUsbPeripheralSpeedLimitingCableNotificationId);
  ASSERT_TRUE(notification);

  // Click the notification to close it.
  notification->delegate()->Click(std::nullopt, std::nullopt);

  // Resend the notification, but expect it not to show after being clicked.
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 0u);
  controller()->OnSpeedLimitingCableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 0u);
}

}  // namespace ash
