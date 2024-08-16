// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/update_notification_controller.h"

#include <optional>

#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/update_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
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
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {
namespace {

const char kNotificationId[] = "chrome://update";
const char* kDomain = "example.com";
const char* kDeviceDomain = "example.org";

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
      /*rollback=*/false);
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

    EnterpriseDomainModel* enterprise_domain =
        Shell::Get()->system_tray_model()->enterprise_domain();
    enterprise_domain->SetEnterpriseAccountDomainInfo(kDomain);
    enterprise_domain->SetDeviceEnterpriseInfo(
        DeviceEnterpriseInfo{kDeviceDomain, ManagementDeviceMode::kNone});
  }

 protected:
  message_center::Notification* GetNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        kNotificationId);
  }

  bool HasNotification() { return GetNotification(); }

  std::string GetNotificationTitle() {
    return base::UTF16ToUTF8(GetNotification()->title());
  }

  std::string GetNotificationMessage() {
    return base::UTF16ToUTF8(GetNotification()->message());
  }

  std::string GetNotificationButtonText(int index) {
    return base::UTF16ToUTF8(GetNotification()->buttons().at(index).title);
  }

  int GetNotificationButtonCount() {
    return GetNotification()->buttons().size();
  }

  int GetNotificationPriority() { return GetNotification()->priority(); }

  const gfx::VectorIcon& GetNotificationIcon() {
    return GetNotification()->vector_small_image();
  }

  bool GetNotificationNeverTimeout() {
    return GetNotification()->never_timeout();
  }

  void AddSlowBootFilePath(const base::FilePath& file_path) {
    bool success = base::WriteFile(file_path, "1\n");
    EXPECT_TRUE(success);
    Shell::Get()
        ->system_notification_controller()
        ->update_->slow_boot_file_path_ = file_path;
  }

  ShutdownConfirmationDialog* GetSlowBootConfirmationDialog() {
    return Shell::Get()
        ->system_notification_controller()
        ->update_->confirmation_dialog_;
  }

  void CompareNotificationColor(SkColor expected_color,
                                ui::ColorId expected_color_id_for_jelly) {
    const auto color_id = GetNotification()->accent_color_id();
    const auto color = GetNotification()->accent_color();

    if (color_id.has_value()) {
      // We use `ui::ColorId` for Jelly.
      EXPECT_EQ(expected_color_id_for_jelly, color_id);
    } else if (color.has_value()) {
      EXPECT_EQ(expected_color, color);
    }
  }

  std::u16string system_app_name_;
};

// Tests that the update icon becomes visible when an update becomes available.
TEST_F(UpdateNotificationControllerTest, VisibilityAfterUpdate) {
  ShowDefaultUpdateNotification();

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorNormal,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysPrimary);
  EXPECT_TRUE(strcmp(kSystemMenuUpdateIcon.name, GetNotificationIcon().name) ==
              0);
  EXPECT_EQ("Update device", GetNotificationTitle());
  EXPECT_EQ("Learn more about the latest " +
                base::UTF16ToUTF8(system_app_name_) + " update",
            GetNotificationMessage());
  EXPECT_EQ("Restart", GetNotificationButtonText(0));

  // Click the restart button.
  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      kNotificationId, 0);

  // Restart was requested.
  EXPECT_EQ(1,
            GetSessionControllerClient()->request_restart_for_update_count());
}

// Tests that the update icon becomes visible when an update becomes
// available.
TEST_F(UpdateNotificationControllerTest, VisibilityAfterUpdateWithSlowReboot) {
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
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorNormal,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysPrimary);
  EXPECT_TRUE(strcmp(kSystemMenuUpdateIcon.name, GetNotificationIcon().name) ==
              0);
  EXPECT_EQ("Update device", GetNotificationTitle());
  EXPECT_EQ("Learn more about the latest " +
                base::UTF16ToUTF8(system_app_name_) +
                " update. This Chromebook needs to restart to apply an update. "
                "This can take up to 1 minute.",
            GetNotificationMessage());
  EXPECT_EQ("Restart", GetNotificationButtonText(0));

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
  // Simulate an update available for downloading over cellular connection.
  Shell::Get()->system_tray_model()->SetUpdateOverCellularAvailableIconVisible(
      true);

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorNormal,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysPrimary);
  EXPECT_TRUE(strcmp(kSystemMenuUpdateIcon.name, GetNotificationIcon().name) ==
              0);
  EXPECT_EQ("Update device", GetNotificationTitle());
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
  // Simulate an update that requires factory reset.
  Shell::Get()->system_tray_model()->ShowUpdateIcon(UpdateSeverity::kLow, true,
                                                    false);

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

  const std::u16string chrome_os_device_name = ui::GetChromeOSDeviceName();

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorNormal,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysPrimary);
  EXPECT_TRUE(strcmp(kSystemMenuUpdateIcon.name, GetNotificationIcon().name) ==
              0);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_TITLE),
            GetNotificationTitle());
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_UPDATE_NOTIFICATION_MESSAGE_POWERWASH,
                                      chrome_os_device_name, system_app_name_),
            GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButtonText(0));
}

TEST_F(UpdateNotificationControllerTest, NoUpdateNotification) {
  // The system starts with no update pending, so the notification isn't
  // visible.
  EXPECT_FALSE(HasNotification());
}

TEST_F(UpdateNotificationControllerTest, RollbackNotification) {
  Shell::Get()->system_tray_model()->ShowUpdateIcon(
      UpdateSeverity::kLow, /*factory_reset_required=*/true,
      /*rollback=*/true);

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

  const std::u16string chrome_os_device_name = ui::GetChromeOSDeviceName();

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorWarning,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysWarning);
  EXPECT_TRUE(
      strcmp(kSystemMenuRollbackIcon.name, GetNotificationIcon().name) == 0);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_ROLLBACK_NOTIFICATION_TITLE),
            GetNotificationTitle());
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_UPDATE_NOTIFICATION_MESSAGE_ROLLBACK,
                                      base::ASCIIToUTF16(kDomain),
                                      chrome_os_device_name),
            GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButtonText(0));
}

TEST_F(UpdateNotificationControllerTest, RollbackRecommendedNotification) {
  Shell::Get()->system_tray_model()->ShowUpdateIcon(
      UpdateSeverity::kLow, /*factory_reset_required=*/true,
      /*rollback=*/true);

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState(
      {.requirement_type = RelaunchNotificationState::kRecommendedNotOverdue});

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

  const std::u16string chrome_os_device_name = ui::GetChromeOSDeviceName();

  // Notification is the same as for a non-recommended rollback.
  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorWarning,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysWarning);
  EXPECT_TRUE(
      strcmp(kSystemMenuRollbackIcon.name, GetNotificationIcon().name) == 0);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_ROLLBACK_NOTIFICATION_TITLE),
            GetNotificationTitle());
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_UPDATE_NOTIFICATION_MESSAGE_ROLLBACK,
                                      base::ASCIIToUTF16(kDomain),
                                      chrome_os_device_name),
            GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButtonText(0));
}

TEST_F(UpdateNotificationControllerTest,
       RollbackRecommendedOverdueNotification) {
  Shell::Get()->system_tray_model()->ShowUpdateIcon(
      UpdateSeverity::kLow, /*factory_reset_required=*/true,
      /*rollback=*/true);

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState(
      {.requirement_type = RelaunchNotificationState::kRecommendedAndOverdue});

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

  const std::u16string chrome_os_device_name = ui::GetChromeOSDeviceName();

  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorCriticalWarning,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysError);
  EXPECT_TRUE(
      strcmp(kSystemMenuRollbackIcon.name, GetNotificationIcon().name) == 0);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_ROLLBACK_OVERDUE_NOTIFICATION_TITLE),
            GetNotificationTitle());
  EXPECT_EQ(l10n_util::GetStringFUTF8(
                IDS_UPDATE_NOTIFICATION_MESSAGE_ROLLBACK_OVERDUE,
                base::ASCIIToUTF16(kDomain), chrome_os_device_name),
            GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButtonText(0));
}

TEST_F(UpdateNotificationControllerTest, RollbackRequiredNotification) {
  Shell::Get()->system_tray_model()->ShowUpdateIcon(
      UpdateSeverity::kLow, /*factory_reset_required=*/true,
      /*rollback=*/true);

  constexpr base::TimeDelta remaining_time = base::Seconds(3);

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState({
      .requirement_type = RelaunchNotificationState::kRequired,
      .rounded_time_until_reboot_required = remaining_time,
  });

  const std::u16string chrome_os_device_name = ui::GetChromeOSDeviceName();

  // Showing Update Notification posts a task to check for slow boot request
  // and use the result of that check to generate appropriate notification. Wait
  // until everything is complete and then check if the notification is visible.
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorCriticalWarning,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysError);
  EXPECT_TRUE(
      strcmp(kSystemMenuRollbackIcon.name, GetNotificationIcon().name) == 0);
  EXPECT_EQ(
      l10n_util::GetPluralStringFUTF8(IDS_ROLLBACK_REQUIRED_TITLE_SECONDS, 3),
      GetNotificationTitle());
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_ROLLBACK_REQUIRED_BODY,
                                      base::ASCIIToUTF16(kDomain),
                                      chrome_os_device_name),
            GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButtonText(0));
}

TEST_F(UpdateNotificationControllerTest, SetUpdateNotificationRecommended) {
  ShowDefaultUpdateNotification();

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState(
      {.requirement_type = RelaunchNotificationState::kRecommendedNotOverdue});

  task_environment()->RunUntilIdle();

  const std::string expected_notification_title =
      l10n_util::GetStringUTF8(IDS_RELAUNCH_RECOMMENDED_TITLE);
  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_RECOMMENDED_BODY, base::ASCIIToUTF16(kDomain));

  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorNormal,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysPrimary);
  EXPECT_TRUE(strcmp(vector_icons::kBusinessIcon.name,
                     GetNotificationIcon().name) == 0);
  EXPECT_EQ(expected_notification_title, GetNotificationTitle());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButtonText(0));
}

TEST_F(UpdateNotificationControllerTest,
       SetUpdateNotificationRecommendedOverdue) {
  ShowDefaultUpdateNotification();

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState(
      {.requirement_type = RelaunchNotificationState::kRecommendedAndOverdue});

  task_environment()->RunUntilIdle();

  const std::string expected_notification_title =
      l10n_util::GetStringUTF8(IDS_RELAUNCH_RECOMMENDED_OVERDUE_TITLE);
  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_RECOMMENDED_OVERDUE_BODY, base::ASCIIToUTF16(kDomain));

  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorNormal,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysPrimary);
  EXPECT_TRUE(strcmp(vector_icons::kBusinessIcon.name,
                     GetNotificationIcon().name) == 0);
  EXPECT_EQ(expected_notification_title, GetNotificationTitle());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButtonText(0));
}

TEST_F(UpdateNotificationControllerTest, SetUpdateNotificationRequiredDays) {
  ShowDefaultUpdateNotification();

  constexpr base::TimeDelta remaining_time = base::Days(3);

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState({
      .requirement_type = RelaunchNotificationState::kRequired,
      .rounded_time_until_reboot_required = remaining_time,
  });

  task_environment()->RunUntilIdle();

  const std::string expected_notification_title =
      l10n_util::GetPluralStringFUTF8(IDS_RELAUNCH_REQUIRED_TITLE_DAYS, 3);
  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_REQUIRED_BODY, base::ASCIIToUTF16(kDomain));

  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorWarning,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysWarning);
  EXPECT_TRUE(strcmp(vector_icons::kBusinessIcon.name,
                     GetNotificationIcon().name) == 0);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, GetNotificationPriority());
  EXPECT_EQ(true, GetNotificationNeverTimeout());
  EXPECT_EQ(expected_notification_title, GetNotificationTitle());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButtonText(0));
}

TEST_F(UpdateNotificationControllerTest,
       SetUpdateNotificationRequiredWithDevicePolicySource) {
  ShowDefaultUpdateNotification();
  AddNotificationWaiter waiter;

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState({
      .requirement_type = RelaunchNotificationState::kRequired,
      .policy_source = RelaunchNotificationState::kDevice,
  });

  waiter.Wait();

  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_REQUIRED_BODY, base::ASCIIToUTF16(kDeviceDomain));

  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
}

TEST_F(UpdateNotificationControllerTest,
       SetUpdateNotificationWithoutAccountDomainManager) {
  ShowDefaultUpdateNotification();
  Shell::Get()
      ->system_tray_model()
      ->enterprise_domain()
      ->SetEnterpriseAccountDomainInfo(std::string());
  AddNotificationWaiter waiter;

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState(
      {.requirement_type = RelaunchNotificationState::kRequired});

  waiter.Wait();

  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_UPDATE_NOTIFICATION_MESSAGE_LEARN_MORE, system_app_name_);

  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
}

TEST_F(UpdateNotificationControllerTest, SetUpdateNotificationRequiredHours) {
  ShowDefaultUpdateNotification();

  constexpr base::TimeDelta remaining_time = base::Hours(3);

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState({
      .requirement_type = RelaunchNotificationState::kRequired,
      .rounded_time_until_reboot_required = remaining_time,
  });

  task_environment()->RunUntilIdle();

  const std::string expected_notification_title =
      l10n_util::GetPluralStringFUTF8(IDS_RELAUNCH_REQUIRED_TITLE_HOURS, 3);
  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_REQUIRED_BODY, base::ASCIIToUTF16(kDomain));

  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorWarning,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysWarning);
  EXPECT_TRUE(strcmp(vector_icons::kBusinessIcon.name,
                     GetNotificationIcon().name) == 0);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, GetNotificationPriority());
  EXPECT_EQ(true, GetNotificationNeverTimeout());
  EXPECT_EQ(expected_notification_title, GetNotificationTitle());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButtonText(0));
}

TEST_F(UpdateNotificationControllerTest, SetUpdateNotificationRequiredMinutes) {
  ShowDefaultUpdateNotification();

  constexpr base::TimeDelta remaining_time = base::Minutes(3);

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState({
      .requirement_type = RelaunchNotificationState::kRequired,
      .rounded_time_until_reboot_required = remaining_time,
  });

  task_environment()->RunUntilIdle();

  const std::string expected_notification_title =
      l10n_util::GetPluralStringFUTF8(IDS_RELAUNCH_REQUIRED_TITLE_MINUTES, 3);
  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_REQUIRED_BODY, base::ASCIIToUTF16(kDomain));

  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorWarning,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysWarning);
  EXPECT_TRUE(strcmp(vector_icons::kBusinessIcon.name,
                     GetNotificationIcon().name) == 0);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, GetNotificationPriority());
  EXPECT_EQ(true, GetNotificationNeverTimeout());
  EXPECT_EQ(expected_notification_title, GetNotificationTitle());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButtonText(0));
}

TEST_F(UpdateNotificationControllerTest, SetUpdateNotificationRequiredSeconds) {
  ShowDefaultUpdateNotification();

  constexpr base::TimeDelta remaining_time = base::Seconds(3);

  Shell::Get()->system_tray_model()->SetRelaunchNotificationState({
      .requirement_type = RelaunchNotificationState::kRequired,
      .rounded_time_until_reboot_required = remaining_time,
  });

  task_environment()->RunUntilIdle();

  const std::string expected_notification_title =
      l10n_util::GetPluralStringFUTF8(IDS_RELAUNCH_REQUIRED_TITLE_SECONDS, 3);
  const std::string expected_notification_body = l10n_util::GetStringFUTF8(
      IDS_RELAUNCH_REQUIRED_BODY, base::ASCIIToUTF16(kDomain));

  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorWarning,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysWarning);
  EXPECT_TRUE(strcmp(vector_icons::kBusinessIcon.name,
                     GetNotificationIcon().name) == 0);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, GetNotificationPriority());
  EXPECT_EQ(true, GetNotificationNeverTimeout());
  EXPECT_EQ(expected_notification_title, GetNotificationTitle());
  EXPECT_EQ(expected_notification_body, GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButtonText(0));
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
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorNormal,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysPrimary);
  EXPECT_TRUE(strcmp(kSystemMenuUpdateIcon.name, GetNotificationIcon().name) ==
              0);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_TITLE),
            GetNotificationTitle());
  EXPECT_EQ(l10n_util::GetStringFUTF8(
                IDS_UPDATE_NOTIFICATION_MESSAGE_LEARN_MORE, system_app_name_),
            GetNotificationMessage());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON),
            GetNotificationButtonText(0));
  EXPECT_NE(message_center::NotificationPriority::SYSTEM_PRIORITY,
            GetNotificationPriority());
}

TEST_F(UpdateNotificationControllerTest,
       VisibilityAfterDeferredUpdateShowNotification) {
  // Simulate a deferred update.
  Shell::Get()->system_tray_model()->SetUpdateDeferred(
      DeferredUpdateState::kShowNotification);

  // Wait until everything is complete and then check if the notification is
  // visible.
  task_environment()->RunUntilIdle();

  // The notification is now visible.
  ASSERT_TRUE(HasNotification());
  CompareNotificationColor(
      /*expected_color=*/kSystemNotificationColorNormal,
      /*expected_color_id_for_jelly=*/cros_tokens::kCrosSysPrimary);
  EXPECT_TRUE(strcmp(kSystemMenuUpdateIcon.name, GetNotificationIcon().name) ==
              0);
  EXPECT_EQ("Update device", GetNotificationTitle());
  EXPECT_EQ(
      "Get the latest features and security improvements. Updates happen in "
      "the background.",
      GetNotificationMessage());
  EXPECT_EQ("Restart", GetNotificationButtonText(0));
}

TEST_F(UpdateNotificationControllerTest,
       VisibilityAfterDeferredUpdateShowDialog) {
  // Simulate a deferred update.
  Shell::Get()->system_tray_model()->SetUpdateDeferred(
      DeferredUpdateState::kShowDialog);

  // Wait until everything is complete and then check if the notification is
  // not visible.
  task_environment()->RunUntilIdle();

  // The notification is not visible.
  ASSERT_FALSE(HasNotification());
}

TEST_F(UpdateNotificationControllerTest, VisibilityAfterDeferredUpdateOff) {
  // Simulate a deferred update.
  Shell::Get()->system_tray_model()->SetUpdateDeferred(
      DeferredUpdateState::kNone);

  // Wait until everything is complete and then check if the notification is
  // not visible.
  task_environment()->RunUntilIdle();

  // The notification is not visible.
  ASSERT_FALSE(HasNotification());
}

}  // namespace ash
