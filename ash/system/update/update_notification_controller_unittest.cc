// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/update_notification_controller.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "ui/message_center/message_center.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define SYSTEM_APP_NAME "Chrome OS"
#else
#define SYSTEM_APP_NAME "Chromium OS"
#endif

namespace ash {

class UpdateNotificationControllerTest : public AshTestBase {
 public:
  UpdateNotificationControllerTest() = default;
  ~UpdateNotificationControllerTest() override = default;

 protected:
  bool HasNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        UpdateNotificationController::kNotificationId);
  }

  std::string GetNotificationTitle() {
    return base::UTF16ToUTF8(
        message_center::MessageCenter::Get()
            ->FindVisibleNotificationById(
                UpdateNotificationController::kNotificationId)
            ->title());
  }

  std::string GetNotificationMessage() {
    return base::UTF16ToUTF8(
        message_center::MessageCenter::Get()
            ->FindVisibleNotificationById(
                UpdateNotificationController::kNotificationId)
            ->message());
  }

  std::string GetNotificationButton(int index) {
    return base::UTF16ToUTF8(
        message_center::MessageCenter::Get()
            ->FindVisibleNotificationById(
                UpdateNotificationController::kNotificationId)
            ->buttons()
            .at(index)
            .title);
  }

  int GetNotificationButtonCount() {
    return message_center::MessageCenter::Get()
        ->FindVisibleNotificationById(
            UpdateNotificationController::kNotificationId)
        ->buttons()
        .size();
  }

  int GetNotificationPriority() {
    return message_center::MessageCenter::Get()
        ->FindVisibleNotificationById(
            UpdateNotificationController::kNotificationId)
        ->priority();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateNotificationControllerTest);
};

// Tests that the update icon becomes visible when an update becomes
// available.
TEST_F(UpdateNotificationControllerTest, VisibilityAfterUpdate) {
  // The system starts with no update pending, so the notification isn't
  // visible.
  EXPECT_FALSE(HasNotification());

  // Simulate an update.
  Shell::Get()->system_tray_model()->ShowUpdateIcon(UpdateSeverity::kLow, false,
                                                    false, UpdateType::kSystem);

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Update available", GetNotificationTitle());
  EXPECT_EQ("Learn more about the latest " SYSTEM_APP_NAME " update",
            GetNotificationMessage());
  EXPECT_EQ("Restart to update", GetNotificationButton(0));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(UpdateNotificationControllerTest, VisibilityAfterFlashUpdate) {
  // The system starts with no update pending, so the notification isn't
  // visible.
  EXPECT_FALSE(HasNotification());

  // Simulate an update.
  Shell::Get()->system_tray_model()->ShowUpdateIcon(UpdateSeverity::kLow, false,
                                                    false, UpdateType::kFlash);

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Adobe Flash Player update available", GetNotificationTitle());
  EXPECT_EQ("Learn more about the latest " SYSTEM_APP_NAME " update",
            GetNotificationMessage());
  EXPECT_EQ("Restart to update", GetNotificationButton(0));
}
#endif

// Tests that the update icon's visibility after an update becomes
// available for downloading over cellular connection.
TEST_F(UpdateNotificationControllerTest,
       VisibilityAfterUpdateOverCellularAvailable) {
  // The system starts with no update pending, so the notification isn't
  // visible.
  EXPECT_FALSE(HasNotification());

  // Simulate an update available for downloading over cellular connection.
  Shell::Get()->system_tray_model()->SetUpdateOverCellularAvailableIconVisible(
      true);

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Update available", GetNotificationTitle());
  EXPECT_EQ("Learn more about the latest " SYSTEM_APP_NAME " update",
            GetNotificationMessage());
  EXPECT_EQ(0, GetNotificationButtonCount());

  // Simulate the user's one time permission on downloading the update is
  // granted.
  Shell::Get()->system_tray_model()->SetUpdateOverCellularAvailableIconVisible(
      false);

  // The notification disappears.
  EXPECT_FALSE(HasNotification());
}

TEST_F(UpdateNotificationControllerTest,
       VisibilityAfterUpdateRequiringFactoryReset) {
  // The system starts with no update pending, so the notification isn't
  // visible.
  EXPECT_FALSE(HasNotification());

  // Simulate an update that requires factory reset.
  Shell::Get()->system_tray_model()->ShowUpdateIcon(UpdateSeverity::kLow, true,
                                                    false, UpdateType::kSystem);

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Update available", GetNotificationTitle());
  EXPECT_EQ(
      "This update requires powerwashing your device."
      " Learn more about the latest " SYSTEM_APP_NAME " update.",
      GetNotificationMessage());
  EXPECT_EQ("Restart to update", GetNotificationButton(0));
}

TEST_F(UpdateNotificationControllerTest, VisibilityAfterRollback) {
  // The system starts with no update pending, so the notification isn't
  // visible.
  EXPECT_FALSE(HasNotification());

  // Simulate a rollback.
  Shell::Get()->system_tray_model()->ShowUpdateIcon(UpdateSeverity::kLow, false,
                                                    true, UpdateType::kSystem);

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Device will be rolled back", GetNotificationTitle());
  EXPECT_EQ(
      "Your administrator is rolling back your device. All data will"
      " be deleted when the device is restarted.",
      GetNotificationMessage());
  EXPECT_EQ("Restart and reset", GetNotificationButton(0));
}

TEST_F(UpdateNotificationControllerTest, SetUpdateNotificationStateTest) {
  // The system starts with no update pending, so the notification isn't
  // visible.
  EXPECT_FALSE(HasNotification());

  // Simulate an update.
  Shell::Get()->system_tray_model()->ShowUpdateIcon(UpdateSeverity::kLow, false,
                                                    false, UpdateType::kSystem);

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Update available", GetNotificationTitle());
  EXPECT_EQ("Learn more about the latest " SYSTEM_APP_NAME " update",
            GetNotificationMessage());
  EXPECT_EQ("Restart to update", GetNotificationButton(0));

  const std::string recommended_notification_title(
      SYSTEM_APP_NAME " will restart in 3 minutes");
  const std::string recommended_notification_body(
      "Your administrator recommended that you restart " SYSTEM_APP_NAME
      " to apply an update");

  // Simulate notification type set to recommended.
  Shell::Get()->system_tray_model()->SetUpdateNotificationState(
      NotificationStyle::kAdminRecommended,
      base::UTF8ToUTF16(recommended_notification_title),
      base::UTF8ToUTF16(recommended_notification_body));

  // The notification's title and body have changed.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(recommended_notification_title, GetNotificationTitle());
  EXPECT_EQ(recommended_notification_body, GetNotificationMessage());
  EXPECT_EQ("Restart to update", GetNotificationButton(0));
  EXPECT_NE(message_center::NotificationPriority::SYSTEM_PRIORITY,
            GetNotificationPriority());

  const std::string required_notification_title(SYSTEM_APP_NAME
                                                " will restart in 3 minutes");
  const std::string required_notification_body(
      "Your administrator required that you restart " SYSTEM_APP_NAME
      " to apply an update");

  // Simulate notification type set to required.
  Shell::Get()->system_tray_model()->SetUpdateNotificationState(
      NotificationStyle::kAdminRequired,
      base::UTF8ToUTF16(required_notification_title),
      base::UTF8ToUTF16(required_notification_body));

  // The notification's title and body have changed.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(required_notification_title, GetNotificationTitle());
  EXPECT_EQ(required_notification_body, GetNotificationMessage());
  EXPECT_EQ("Restart to update", GetNotificationButton(0));
  // The admin required relaunch notification has system priority.
  EXPECT_EQ(message_center::NotificationPriority::SYSTEM_PRIORITY,
            GetNotificationPriority());

  // Simulate notification type set back to default.
  Shell::Get()->system_tray_model()->SetUpdateNotificationState(
      NotificationStyle::kDefault, base::string16(), base::string16());

  // The notification has the default text.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Update available", GetNotificationTitle());
  EXPECT_EQ("Learn more about the latest " SYSTEM_APP_NAME " update",
            GetNotificationMessage());
  EXPECT_EQ("Restart to update", GetNotificationButton(0));
  EXPECT_NE(message_center::NotificationPriority::SYSTEM_PRIORITY,
            GetNotificationPriority());
}

}  // namespace ash
