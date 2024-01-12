// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/chrome_file_manager_ui_delegate.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/file_manager_string_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/file_errors.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

ChromeFileManagerUIDelegate::ChromeFileManagerUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {
  DCHECK(web_ui_);
}

ChromeFileManagerUIDelegate::~ChromeFileManagerUIDelegate() = default;

base::Value::Dict ChromeFileManagerUIDelegate::GetLoadTimeData() const {
  base::Value::Dict dict = GetFileManagerStrings();

  const std::string locale = g_browser_process->GetApplicationLocale();
  AddFileManagerFeatureStrings(locale, Profile::FromWebUI(web_ui_), &dict);
  return dict;
}

std::unique_ptr<content::WebUIMessageHandler>
ChromeFileManagerUIDelegate::GetPluralStringHandler() const {
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  for (const auto [key, value] : GetFileManagerPluralStrings()) {
    plural_string_handler->AddLocalizedString(key, value.GetInt());
  }
  return plural_string_handler;
}

void ChromeFileManagerUIDelegate::ProgressPausedTasks() const {
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(Profile::FromWebUI(web_ui_));

  if (volume_manager && volume_manager->io_task_controller()) {
    volume_manager->io_task_controller()->ProgressPausedTasks();
  }
}

void ChromeFileManagerUIDelegate::ShouldPollDriveHostedPinStates(bool enabled) {
  if (poll_hosted_pin_states_ == enabled) {
    return;
  }
  poll_hosted_pin_states_ = enabled;
  if (enabled) {
    PollHostedPinStates();
  }
}

void ChromeFileManagerUIDelegate::ShowPolicyNotifications() const {
  policy::FilesPolicyNotificationManager* fpnm =
      policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui_));
  if (fpnm) {
    fpnm->ShowBlockedNotifications();
  }
}

void ChromeFileManagerUIDelegate::PollHostedPinStates() {
  if (!poll_hosted_pin_states_) {
    return;
  }
  if (drive::DriveIntegrationService* const service =
          drive::util::GetIntegrationServiceByProfile(
              Profile::FromWebUI(web_ui_))) {
    VLOG(1) << "Polling hosted file pin states";
    service->PollHostedFilePinStates();
  }

  base::TimeDelta poll_delay =
      drive::util::IsDriveFsBulkPinningAvailable(Profile::FromWebUI(web_ui_))
          ? base::Seconds(15)
          : base::Minutes(3);

  // After the `poll_delay` call `GetDocsOfflineStats`, the
  // `PollHostedFilePinStates` function caches the number of items pinned /
  // available offline, ensure there's enough time for that data to be
  // retrieved.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ChromeFileManagerUIDelegate::PollDocsOfflineStats,
                     weak_ptr_factory_.GetWeakPtr(), poll_delay),
      poll_delay);
}

void ChromeFileManagerUIDelegate::PollDocsOfflineStats(
    const base::TimeDelta poll_delay) {
  if (drive::DriveIntegrationService* const service =
          drive::util::GetIntegrationServiceByProfile(
              Profile::FromWebUI(web_ui_))) {
    VLOG(1) << "Getting docs offline stats";
    service->GetDocsOfflineStats(
        base::BindOnce(&ChromeFileManagerUIDelegate::RecordDocsOfflineStats,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ChromeFileManagerUIDelegate::PollHostedPinStates,
                     weak_ptr_factory_.GetWeakPtr()),
      poll_delay);
}

void ChromeFileManagerUIDelegate::RecordDocsOfflineStats(
    drive::FileError error,
    drivefs::mojom::DocsOfflineStatsPtr stats) {
  if (error != drive::FileError::FILE_ERROR_OK || !stats ||
      stats->total == -1 || stats->available_offline == -1) {
    VLOG(1) << "Not recording the Docs offline UMA stat";
    return;
  }

  const int32_t availability_percent =
      stats->total == 0 ? 0 : (stats->available_offline * 100 / stats->total);

  // Only emitted if a change in percentage is detected.
  if (total_available_offline_hosted_files_ != stats->available_offline ||
      total_hosted_files_ != stats->total) {
    base::UmaHistogramPercentage(
        "FileBrowser.GoogleDrive.DSSAvailabilityPercentage",
        availability_percent);
  }

  // Emitted after 1 day has elapsed since it was last emitted.
  if (Profile* const profile = Profile::FromWebUI(web_ui_)) {
    PrefService* const prefs = profile->GetPrefs();
    DCHECK(prefs);

    const base::TimeDelta time_since_last_update =
        base::Time::Now() -
        prefs->GetTime(drive::prefs::kDriveFsDSSAvailabilityLastEmitted);
    if (time_since_last_update > base::Days(1)) {
      base::UmaHistogramPercentage(
          "FileBrowser.GoogleDrive.DailyDSSAvailabilityPercentage",
          availability_percent);
    }

    if (time_since_last_update > base::Days(1) ||
        time_since_last_update.is_negative()) {
      prefs->SetTime(drive::prefs::kDriveFsDSSAvailabilityLastEmitted,
                     base::Time::Now());
    }
  }

  total_available_offline_hosted_files_ = stats->available_offline;
  total_hosted_files_ = stats->total;
}
