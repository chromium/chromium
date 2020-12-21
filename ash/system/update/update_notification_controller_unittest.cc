// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/update_notification_controller.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/session/shutdown_confirmation_dialog.h"
#include "ash/system/system_notification_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define SYSTEM_APP_NAME "Chrome OS"
#else
#define SYSTEM_APP_NAME "Chromium OS"
#endif

namespace ash {
namespace {

const char kNotificationId[] = "chrome://update";

// Waits for the notification to be added. Needed because the controller posts a
// task to check for slow boot request before showing the notification.
class AddNotificationWaiter : public message_center::MessageCenterObserver {
 public:
  AddNotificationWaiter() {
    message_center::MessageCenter::Get()->AddObserver(this);
  }
  ~AddNotificationWaiter() override {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override {
    if (notification_id == kNotificationId)
      run_loop_.Quit();
  }

  base::RunLoop run_loop_;
};

}  // namespace

class UpdateNotificationControllerTest : public AshTestBase {
 public:
  UpdateNotificationControllerTest() = default;
  ~UpdateNotificationControllerTest() override = default;

 protected:
  bool HasNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        kNotificationId);
  }

  std::string GetNotificationTitle() {
    return base::UTF16ToUTF8(message_center::MessageCenter::Get()
                                 ->FindVisibleNotificationById(kNotificationId)
                                 ->title());
  }

  std::string GetNotificationMessage() {
    return base::UTF16ToUTF8(message_center::MessageCenter::Get()
                                 ->FindVisibleNotificationById(kNotificationId)
                                 ->message());
  }

  std::string GetNotificationButton(int index) {
    return base::UTF16ToUTF8(message_center::MessageCenter::Get()
                                 ->FindVisibleNotificationById(kNotificationId)
                                 ->buttons()
                                 .at(index)
                                 .title);
  }

  int GetNotificationButtonCount() {
    return message_center::MessageCenter::Get()
        ->FindVisibleNotificationById(kNotificationId)
        ->buttons()
        .size();
  }

  int GetNotificationPriority() {
    return message_center::MessageCenter::Get()
        ->FindVisibleNotificationById(kNotificationId)
        ->priority();
  }

  void AddSlowBootFilePath(const base::FilePath& file_path) {
    int bytes_written = base::WriteFile(file_path, "1\n", 2);
    EXPECT_TRUE(bytes_written == 2);
    Shell::Get()
        ->system_notification_controller()
        ->update_->slow_boot_file_path_ = file_path;
  }

  ShutdownConfirmationDialog* GetSlowBootConfirmationDialog() {
    return Shell::Get()
        ->system_notification_controller()
        ->update_->confirmation_dialog_;
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

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Update available", GetNotificationTitle());
  EXPECT_EQ("Learn more about the latest " SYSTEM_APP_NAME " update",
            GetNotificationMessage());
  EXPECT_EQ("Restart to update", GetNotificationButton(0));
}

// Tests that the update icon becomes visible when an update becomes
// available.
TEST_F(UpdateNotificationControllerTest, VisibilityAfterUpdateWithSlowReboot) {
  // The system starts with no update pending, so the notification isn't
  // visible.
  EXPECT_FALSE(HasNotification());

  // Add a slow boot file.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  AddSlowBootFilePath(tmp_dir.GetPath().Append("slow_boot_required"));

  // Simulate an update.
  Shell::Get()->system_tray_model()->ShowUpdateIcon(UpdateSeverity::kLow, false,
                                                    false, UpdateType::kSystem);

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Update available", GetNotificationTitle());
  EXPECT_EQ("Learn more about the latest " SYSTEM_APP_NAME
            " update. This Chromebook needs to restart to apply an update. "
            "This can take up to 1 minute.",
            GetNotificationMessage());
  EXPECT_EQ("Restart to update", GetNotificationButton(0));

  // Ensure Slow Boot Dialog is not open.
  EXPECT_FALSE(GetSlowBootConfirmationDialog());

  // Trigger Click on "Restart to Update" button in Notification.
  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      kNotificationId, 0);

  // Ensure Slow Boot Dialog is open and notification is removed.
  ASSERT_TRUE(GetSlowBootConfirmationDialog());
  EXPECT_FALSE(HasNotification());

  // Click the cancel button on Slow Boot Confirmation Dialog.
  GetSlowBootConfirmationDialog()->CancelDialog();

  // Ensure that the Slow Boot Dialog is closed and notification is visible.
  EXPECT_FALSE(GetSlowBootConfirmationDialog());
  EXPECT_TRUE(HasNotification());
}

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

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

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

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

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

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

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

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

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

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

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

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

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

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

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

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

  // The notification has the default text.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Update available", GetNotificationTitle());
  EXPECT_EQ("Learn more about the latest " SYSTEM_APP_NAME " update",
            GetNotificationMessage());
  EXPECT_EQ("Restart to update", GetNotificationButton(0));
  EXPECT_NE(message_center::NotificationPriority::SYSTEM_PRIORITY,
            GetNotificationPriority());
}

TEST_F(UpdateNotificationControllerTest, VisibilityAfterLacrosUpdate) {
  // The system starts with no update pending, so the notification isn't
  // visible.
  EXPECT_FALSE(HasNotification());

  // Simulate an update.
  AddNotificationWaiter waiter;
  Shell::Get()->system_tray_model()->ShowUpdateIcon(UpdateSeverity::kLow, false,
                                                    false, UpdateType::kLacros);
  waiter.Wait();

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Lacros update available", GetNotificationTitle());
  EXPECT_EQ("Device restart is required to apply the update.",
            GetNotificationMessage());
  EXPECT_EQ("Restart to update", GetNotificationButton(0));
}

}  // namespace ash
