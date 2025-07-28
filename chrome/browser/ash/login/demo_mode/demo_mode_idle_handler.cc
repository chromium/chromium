// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_idle_handler.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/metrics/demo_session_metrics_recorder.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drive_integration_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/experiences/idle_detector/idle_detector.h"
#include "components/drive/file_system_core_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

// Amount of idle time for re-launch demo mode swa with demo account login.
// TODO(crbug.com/380941267): Use a policy to control this the idle duration.
constexpr base::TimeDelta kReLuanchDemoAppIdleDuration = base::Seconds(90);

// The range of logout delay for current fallback MGS.
// TODO(crbugs.com/355727308): Get logout delay from server response.
constexpr base::TimeDelta kLogoutDelayMin = base::Minutes(60);
constexpr base::TimeDelta kLogoutDelayMax = base::Minutes(90);

constexpr char kDemoSessionToSNotificationId[] = "demo_session_ToS";
constexpr char kGooglePoliciesURL[] = "https://policies.google.com/";

// The list of prefs that are reset on the start of each shopper session.
const char* const kPrefsPrefixToReset[] = {
    "settings.audio", prefs::kPowerAcScreenBrightnessPercent,
    prefs::kPowerBatteryScreenBrightnessPercent};

void ResetWallpaper() {
  auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager) {
    // This can happen in tests or shutdown.
    return;
  }

  const auto* primary_user = user_manager->GetPrimaryUser();
  WallpaperController::Get()->SetDefaultWallpaper(primary_user->GetAccountId(),
                                                  /*show_wallpaper=*/true,
                                                  base::DoNothing());
}

void ResetPrefs() {
  auto* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);
  auto* prefs = profile->GetPrefs();
  CHECK(prefs);

  for (auto* const pref : kPrefsPrefixToReset) {
    prefs->ClearPrefsWithPrefixSilently(pref);
  }
}

void DeleteAllFilesUnderPath(const base::FilePath& directory_path) {
  base::FileEnumerator e(
      directory_path, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file_path = e.Next(); !file_path.empty();
       file_path = e.Next()) {
    if (!base::DeletePathRecursively(file_path)) {
      PLOG(ERROR) << "Cannot delete '" << file_path << "'";
    }
  }
}

// Deletes all user created files in DriveFS.
void CleanUpDriveFs(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          ProfileManager::GetActiveUserProfile());

  // In managed guest session demo mode, there's no DriveFS.
  if (!integration_service) {
    return;
  }

  const base::FilePath root = integration_service->GetMountPointPath().Append(
      base::FilePath(drive::util::kDriveMyDriveRootDirName));
  blocking_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DeleteAllFilesUnderPath, root));
}

void LogoutCurrentSession() {
  if (Shell::HasInstance()) {
    SYSLOG(INFO)
        << "Logout current demo mode session for retrying sign in demo account";
    Shell::Get()->session_controller()->RequestSignOut();
  }
}

}  // namespace

DemoModeIdleHandler::DemoModeIdleHandler(
    DemoModeWindowCloser* window_closer,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : window_closer_(window_closer),
      blocking_task_runner_(blocking_task_runner),
      file_cleaner_(blocking_task_runner) {
  user_activity_observer_.Observe(ui::UserActivityDetector::Get());

  // Maybe schedule a logout for current session. The timer will be reset if
  // there's any user activity.
  if (demo_mode::GetShouldScheduleLogoutForMGS()) {
    mgs_logout_timer_.emplace();
    SYSLOG(INFO) << "Start Logout timer to retry login with demo account.";
    mgs_logout_timer_->Start(
        FROM_HERE, base::RandTimeDelta(kLogoutDelayMin, kLogoutDelayMax),
        base::BindOnce(&LogoutCurrentSession));
  }
}

DemoModeIdleHandler::~DemoModeIdleHandler() = default;

void ShowNotification() {
  const std::u16string notification_title =
      l10n_util::GetStringUTF16(IDS_DEMO_SESSION_TOS_NOTIFICATION_TITLE);
  const std::u16string notification_message =
      l10n_util::GetStringUTF16(IDS_DEMO_SESSION_TOS_NOTIFICATION_MESSAGE);
  const std::u16string button_text =
      l10n_util::GetStringUTF16(IDS_DEMO_SESSION_TOS_NOTIFICATION_BUTTON_TEXT);

  message_center::RichNotificationData optional_fields;
  // Set a higher priority to make sure it displays even with Do Not Disturb
  // (DND) enabled.
  optional_fields.priority =
      message_center::NotificationPriority::SYSTEM_PRIORITY;
  // Add "Learn more" button
  optional_fields.buttons.emplace_back(message_center::ButtonInfo(button_text));

  const gfx::VectorIcon& privacy_indicator_icon = kPrivacyIndicatorsIcon;

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kDemoSessionToSNotificationId, notification_title,
          notification_message,
          /*display_source=*/std::u16string(),
          /*origin_url=*/GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kDemoSessionToSNotificationId,
              ash::NotificationCatalogName::kDemoMode),
          optional_fields,
          // Open the URL when the button is clicked
          // https://policies.google.com/
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating([](std::optional<int> button_index) {
                if (button_index.has_value() && button_index.value() == 0) {
                  ash::NewWindowDelegate::GetInstance()->OpenUrl(
                      GURL(kGooglePoliciesURL),
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
                }
              })),
          privacy_indicator_icon,
          // Demo mode has DND turned on by default, we need the warning level
          // to be CRITICAL_WARNING to display the notification.
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);

  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);

  message_center->RemoveNotification(notification->id(), /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

void DemoModeIdleHandler::OnUserActivity(const ui::Event* event) {
  // If there's user activity, no need `mgs_logout_timer_` any more. Device will
  // auto logout after 90s idle if logout is required.
  if (mgs_logout_timer_ && mgs_logout_timer_->IsRunning()) {
    mgs_logout_timer_.reset();
  }

  // Check whether the power policy has been overridden and a 24 hour session is
  // enable. If the 24h session is disabled, return early. The power policy will
  // detect device idle and handle session behaviour.
  if (!demo_mode::ForceSessionLengthCountFromSessionStarts()) {
    return;
  }

  // We only start the `idle_detector_` timer on the first user activity. If
  // the user is already active, we don't need to do this again.
  if (is_user_active_) {
    return;
  }

  CHECK(!idle_detector_);
  is_user_active_ = true;

  // The idle detector also observes user activity and it resets its timer if it
  // is less than `kReLuanchDemoAppIdleDuration`.
  idle_detector_ = std::make_unique<IdleDetector>(
      base::BindRepeating(&DemoModeIdleHandler::OnIdle,
                          weak_ptr_factory_.GetWeakPtr()),
      /*tick_clock=*/nullptr);

  if (features::IsDemoSessionToSNotificationEnabled()) {
    ShowNotification();
  }

  idle_detector_->Start(
      idle_time_out_for_test_.value_or(kReLuanchDemoAppIdleDuration));
}

void DemoModeIdleHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DemoModeIdleHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DemoModeIdleHandler::SetIdleTimeoutForTest(
    std::optional<base::TimeDelta> timeout) {
  idle_time_out_for_test_ = std::move(timeout);
}

// This function is invoked on the task runner of timer. Post task properly on
// different thread.
void DemoModeIdleHandler::OnIdle() {
  // Report shopper session dwell time metrics.
  DemoSessionMetricsRecorder::Get()->ReportShopperSessionDwellTime();

  // Stop idle detect clock:
  idle_detector_.reset();
  is_user_active_ = false;

  if (features::IsDemoModeSignInFileCleanupEnabled()) {
    // The IO tasks will be executed from non-UI thread by  `file_cleaner_`.
    CleanupLocalFiles();
    CleanUpDriveFs(blocking_task_runner_);
  }

  window_closer_->StartClosingApps();
  ResetPrefs();

  // Explicitly call to set default wallpaper. Clear wallpaper prefs doesn't
  // change the UI.
  ResetWallpaper();

  // TODO(crbug.com/382360715): Restore network if changed by user.
}

void DemoModeIdleHandler::CleanupLocalFiles() {
  // TODO(crbug.com/396731796): Maybe only do clean up when there's a change
  // under "MyFiles". Unmount the mounted archives before the cleanup. Restore
  // the default download location if user changed it.

  // Note this won't work for emulator since "MyFiles" is not mounted from user
  // data directory.
  file_cleaner_.Cleanup(
      base::BindOnce(&DemoModeIdleHandler::OnLocalFilesCleanupCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoModeIdleHandler::OnLocalFilesCleanupCompleted(
    const std::optional<std::string>& error_message) {
  if (error_message) {
    LOG(ERROR) << "Cleanup local files on device idle failed: "
               << error_message.value();
    return;
  }

  observers_.Notify(&Observer::OnLocalFilesCleanupCompleted);
}

}  // namespace ash
