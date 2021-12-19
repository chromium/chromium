// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/usb_peripheral/usb_peripheral_notification_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;

namespace ash {

namespace {

const char kUsbPeripheralInvalidDpCableNotificationId[] =
    "cros_usb_peripheral_invalid_dp_cable_notification_id";

}  // namespace

class UsbPeripheralNotificationControllerTest : public AshTestBase {
 public:
  UsbPeripheralNotificationControllerTest() {}
  UsbPeripheralNotificationControllerTest(
      const UsbPeripheralNotificationControllerTest&) = delete;
  UsbPeripheralNotificationControllerTest& operator=(
      const UsbPeripheralNotificationControllerTest&) = delete;
  ~UsbPeripheralNotificationControllerTest() override = default;

  UsbPeripheralNotificationController* controller() {
    return Shell::Get()->usb_peripheral_notification_controller();
  }

  message_center::Notification* GetInvalidDpCableNotification() {
    return MessageCenter::Get()->FindVisibleNotificationById(
        kUsbPeripheralInvalidDpCableNotificationId);
  }
};

TEST_F(UsbPeripheralNotificationControllerTest, InvalidDpCableNotification) {
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 0u);
  controller()->OnInvalidDpCableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);

  message_center::Notification* notification = GetInvalidDpCableNotification();
  ASSERT_TRUE(notification);

  EXPECT_EQ(notification->buttons().size(), 0u);
  controller()->OnInvalidDpCableWarning();
  EXPECT_EQ(MessageCenter::Get()->NotificationCount(), 1u);
}

}  // namespace ash
