// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/firmware_update/firmware_update_notification_controller.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

using message_center::MessageCenter;

namespace ash {

namespace {
const char kFirmwareUpdateNotificationId[] =
    "cros_firmware_update_notification_id";

}  // namespace

class FirmwareUpdateNotificationControllerTest : public AshTestBase {
 public:
  FirmwareUpdateNotificationControllerTest() = default;
  FirmwareUpdateNotificationControllerTest(
      const FirmwareUpdateNotificationControllerTest&) = delete;
  FirmwareUpdateNotificationControllerTest& operator=(
      const FirmwareUpdateNotificationControllerTest&) = delete;
  ~FirmwareUpdateNotificationControllerTest() override = default;

  FirmwareUpdateNotificationController* controller() {
    return Shell::Get()->firmware_update_notification_controller();
  }

  message_center::Notification* GetFirmwareUpdateNotification() {
    return MessageCenter::Get()->FindVisibleNotificationById(
        kFirmwareUpdateNotificationId);
  }

  int GetNumFirmwareUpdateUIOpened() {
    return GetSystemTrayClient()->show_firmware_update_count();
  }

  void ClickNotification(absl::optional<int> button_index) {
    // No button index means the notification body was clicked.
    if (!button_index.has_value()) {
      message_center::Notification* notification =
          MessageCenter::Get()->FindVisibleNotificationById(
              kFirmwareUpdateNotificationId);
      notification->delegate()->Click(absl::nullopt, absl::nullopt);
      return;
    }

    message_center::Notification* notification =
        MessageCenter::Get()->FindVisibleNotificationById(
            kFirmwareUpdateNotificationId);
    notification->delegate()->Click(button_index, absl::nullopt);
  }
};

TEST_F(FirmwareUpdateNotificationControllerTest, FirmwareUpdateNotification) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  controller()->NotifyFirmwareUpdateAvailable();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  message_center::Notification* notification = GetFirmwareUpdateNotification();

  EXPECT_TRUE(notification);

  // Ensure this notification has one button.
  EXPECT_EQ(1u, notification->buttons().size());

  EXPECT_EQ(0, GetNumFirmwareUpdateUIOpened());
  // Click on the update button and expect it to open the Firmware Update
  // SWA.
  ClickNotification(/*button_index=*/0);
  EXPECT_EQ(1, GetNumFirmwareUpdateUIOpened());
  // Clicking on the notification will close it.
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  // Open new notification and click on its body.
  controller()->NotifyFirmwareUpdateAvailable();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());
  ClickNotification(absl::nullopt);
  EXPECT_EQ(2, GetNumFirmwareUpdateUIOpened());
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
}

}  // namespace ash
