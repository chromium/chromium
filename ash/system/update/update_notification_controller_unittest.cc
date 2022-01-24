// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/update_notification_controller.h"

#include "ash/public/cpp/update_types.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/enterprise_domain_model.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {
namespace {

const char kNotificationId[] = "chrome://update";
const char* kDomain = "google.com";

// Waits for the notification to be added. Needed because the controller
// posts a task to check for slow boot request before showing the
// notification.
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

void ShowDefaultUpdateNotification() {
  Shell::Get()->system_tray_model()->ShowUpdateIcon(
      UpdateSeverity::kLow, /*factory_reset_required=*/false,
      /*rollback=*/false, UpdateType::kSystem);
}

}  // namespace

class UpdateNotificationControllerTest : public AshTestBase {
 public:
  UpdateNotificationControllerTest() = default;

  UpdateNotificationControllerTest(const UpdateNotificationControllerTest&) =
      delete;
  UpdateNotificationControllerTest& operator=(
      const UpdateNotificationControllerTest&) = delete;

  ~UpdateNotificationControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    system_app_name_ =
        l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME);

    Shell::Get()
        ->system_tray_model()
        ->enterprise_domain()
        ->SetEnterpriseDomainInfo(kDomain, false);
  }

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

  std::u16string system_app_name_;
};

// Tests that the update icon becomes visible when an update becomes
// available.
TEST_F(UpdateNotificationControllerTest, VisibilityAfterUpdate) {
  // The system starts with no update pending, so the notification isn't
  // visible.
  EXPECT_FALSE(HasNotification());

  ShowDefaultUpdateNotification();

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Update available", GetNotificationTitle());
  EXPECT_EQ("Learn more about the latest " +
                base::UTF16ToUTF8(system_app_name_) + " update",
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

  ShowDefaultUpdateNotification();

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ("Update available", GetNotificationTitle());
  EXPECT_EQ("Learn more about the latest " +
                base::UTF16ToUTF8(system_app_name_) +
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
  EXPECT_EQ("Learn more about the latest " +
                base::UTF16ToUTF8(system_app_name_) + " update",
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
      " Learn more about the latest " +
          base::UTF16ToUTF8(system_app_name_) + " update.",
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

TEST_F(UpdateNotificationControllerTest, NoUpdateNotification) {
  // The system starts with no update pending, so the notification isn't
  // visible.
  EXPECT_FALSE(HasNotification());
}

TEST_F(UpdateNotificationControllerTest, SetUpdateNotificationRecommended) {
  ShowDefaultUpdateNotification();

  const std::u16string chrome_os_device_name = ui::GetChromeOSDeviceName();

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState(
      {.requirement_type = RelaunchNotificationState::kRecommendedNotOverdue});

  task_environment()->RunUntilIdle();

  const std::string expected_notification_title =
      l10n_util::GetStringUTF8(IDS_RELAUNCH_RECOMMENDED_TITLE);
  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_RECOMMENDED_BODY, base::ASCIIToUTF16(kDomain),
      chrome_os_device_name);

  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(expected_notification_title, GetNotificationTitle());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButton(0));
}

TEST_F(UpdateNotificationControllerTest,
       SetUpdateNotificationRecommendedOverdue) {
  ShowDefaultUpdateNotification();

  const std::u16string chrome_os_device_name = ui::GetChromeOSDeviceName();

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState(
      {.requirement_type = RelaunchNotificationState::kRecommendedAndOverdue});

  task_environment()->RunUntilIdle();

  const std::string expected_notification_title =
      l10n_util::GetStringUTF8(IDS_RELAUNCH_RECOMMENDED_OVERDUE_TITLE);
  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_RECOMMENDED_OVERDUE_BODY, base::ASCIIToUTF16(kDomain),
      chrome_os_device_name);

  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(expected_notification_title, GetNotificationTitle());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButton(0));
}

TEST_F(UpdateNotificationControllerTest, SetUpdateNotificationRequiredDays) {
  ShowDefaultUpdateNotification();

  const std::u16string chrome_os_device_name = ui::GetChromeOSDeviceName();
  constexpr base::TimeDelta remaining_time = base::Days(3);

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState({
      .requirement_type = RelaunchNotificationState::kRequired,
      .rounded_time_until_reboot_required = remaining_time,
  });

  task_environment()->RunUntilIdle();

  const std::string expected_notification_title =
      l10n_util::GetPluralStringFUTF8(IDS_RELAUNCH_REQUIRED_TITLE_DAYS, 3);
  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_REQUIRED_BODY, base::ASCIIToUTF16(kDomain),
      chrome_os_device_name);

  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(expected_notification_title, GetNotificationTitle());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButton(0));
}

TEST_F(UpdateNotificationControllerTest, SetUpdateNotificationRequiredHours) {
  ShowDefaultUpdateNotification();

  const std::u16string chrome_os_device_name = ui::GetChromeOSDeviceName();
  constexpr base::TimeDelta remaining_time = base::Hours(3);

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState({
      .requirement_type = RelaunchNotificationState::kRequired,
      .rounded_time_until_reboot_required = remaining_time,
  });

  task_environment()->RunUntilIdle();

  const std::string expected_notification_title =
      l10n_util::GetPluralStringFUTF8(IDS_RELAUNCH_REQUIRED_TITLE_HOURS, 3);
  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_REQUIRED_BODY, base::ASCIIToUTF16(kDomain),
      chrome_os_device_name);

  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(expected_notification_title, GetNotificationTitle());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButton(0));
}

TEST_F(UpdateNotificationControllerTest, SetUpdateNotificationRequiredMinutes) {
  ShowDefaultUpdateNotification();

  const std::u16string chrome_os_device_name = ui::GetChromeOSDeviceName();
  constexpr base::TimeDelta remaining_time = base::Minutes(3);

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState({
      .requirement_type = RelaunchNotificationState::kRequired,
      .rounded_time_until_reboot_required = remaining_time,
  });

  task_environment()->RunUntilIdle();

  const std::string expected_notification_title =
      l10n_util::GetPluralStringFUTF8(IDS_RELAUNCH_REQUIRED_TITLE_MINUTES, 3);
  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_REQUIRED_BODY, base::ASCIIToUTF16(kDomain),
      chrome_os_device_name);

  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(expected_notification_title, GetNotificationTitle());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButton(0));
}

TEST_F(UpdateNotificationControllerTest, SetUpdateNotificationRequiredSeconds) {
  ShowDefaultUpdateNotification();

  const std::u16string chrome_os_device_name = ui::GetChromeOSDeviceName();
  constexpr base::TimeDelta remaining_time = base::Seconds(3);

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState({
      .requirement_type = RelaunchNotificationState::kRequired,
      .rounded_time_until_reboot_required = remaining_time,
  });

  task_environment()->RunUntilIdle();

  const std::string expected_notification_title =
      l10n_util::GetPluralStringFUTF8(IDS_RELAUNCH_REQUIRED_TITLE_SECONDS, 3);
  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_REQUIRED_BODY, base::ASCIIToUTF16(kDomain),
      chrome_os_device_name);

  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(expected_notification_title, GetNotificationTitle());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButton(0));
}

// Simulates setting the notification back to the default after showing
// one for recommended updates.
TEST_F(UpdateNotificationControllerTest, SetBackToDefault) {
  ShowDefaultUpdateNotification();

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState(
      {.requirement_type = RelaunchNotificationState::kRecommendedNotOverdue});

  task_environment()->RunUntilIdle();

  // Reset update state.
  Shell::Get()->system_tray_model()->SetRelaunchNotificationState({});

  task_environment()->RunUntilIdle();

  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_TITLE),
            GetNotificationTitle());
  EXPECT_EQ(l10n_util::GetStringFUTF8(
                IDS_UPDATE_NOTIFICATION_MESSAGE_LEARN_MORE, system_app_name_),
            GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButton(0));
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

  // Click the "Restart to update" button.
  message_center::MessageCenter::Get()
      ->FindVisibleNotificationById(kNotificationId)
      ->delegate()
      ->Click(/*button_index=*/0, /*reply=*/absl::nullopt);

  // Controller tried to restart chrome.
  EXPECT_EQ(1, GetSessionControllerClient()->attempt_restart_chrome_count());
}

}  // namespace ash
