// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"

#include <memory>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/policy/skyvault/local_user_files_policy_observer.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace policy::local_user_files {

namespace {

// Delay the migration for 24 hours.
const base::TimeDelta kMigrationTimeout = base::Hours(24);

}  // namespace

LocalFilesMigrationManager::LocalFilesMigrationManager()
    : notification_manager_(std::make_unique<MigrationNotificationManager>()),
      start_delay_timer_(std::make_unique<base::WallClockTimer>()) {
  pref_change_registrar_.Init(g_browser_process->local_state());
  pref_change_registrar_.Add(
      prefs::kLocalUserFilesMigrationEnabled,
      base::BindRepeating(
          &LocalFilesMigrationManager::OnLocalUserFilesPolicyChanged,
          base::Unretained(this)));
}

LocalFilesMigrationManager::~LocalFilesMigrationManager() {
  pref_change_registrar_.RemoveAll();
}

void LocalFilesMigrationManager::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void LocalFilesMigrationManager::RemoveObserver(Observer* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

void LocalFilesMigrationManager::OnLocalUserFilesPolicyChanged() {
  bool local_user_files_allowed = LocalUserFilesAllowed();
  bool local_user_files_migration_enabled =
      g_browser_process->local_state()->GetBoolean(
          prefs::kLocalUserFilesMigrationEnabled);

  if (local_user_files_allowed_ != local_user_files_allowed ||
      local_user_files_migration_enabled_ !=
          local_user_files_migration_enabled) {
    local_user_files_allowed_ = local_user_files_allowed;
    local_user_files_migration_enabled_ = local_user_files_migration_enabled;
    MaybeMigrateFiles(
        base::BindOnce(&LocalFilesMigrationManager::OnMigrationDone,
                       weak_factory_.GetWeakPtr()));
  }
}

bool LocalFilesMigrationManager::ShouldStart() {
  if (!base::FeatureList::IsEnabled(features::kSkyVaultV2)) {
    return false;
  }

  // Migration is enabled only if local files are disabled and the migration
  // policy is set to true...
  if (local_user_files_allowed_ || !local_user_files_migration_enabled_) {
    // TODO(aidazolic): Stop migration if the policy resets?
    return false;
  }

  // ... and the FilesAppDefaultLocation (derived from DownloadDirectory) is set
  // to Google Drive or OneDrive.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  CHECK(profile);
  const PrefService* const prefs = profile->GetPrefs();
  CHECK(prefs);
  const std::string defaultLocation =
      prefs->GetString(prefs::kFilesAppDefaultLocation);
  const bool download_directory_set =
      defaultLocation == download_dir_util::kLocationGoogleDrive ||
      defaultLocation == download_dir_util::kLocationOneDrive;
  if (!download_directory_set) {
    // SkyVault is misconfigured.
    // TODO(aidazolic): Stop migration if the policy resets?
    // TODO(aidazolic): Show an error notification if there are any files.
    return false;
  }

  if (in_progress_) {
    return false;
  }

  return true;
}

void LocalFilesMigrationManager::MaybeMigrateFiles(base::OnceClosure callback) {
  if (!ShouldStart()) {
    return;
  }
  // TODO(aidazolic): Show the dialog.
  start_delay_timer_->Start(
      FROM_HERE, base::Time::Now() + kMigrationTimeout,
      base::BindOnce(&LocalFilesMigrationManager::StartMigration,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalFilesMigrationManager::StartMigration(base::OnceClosure callback) {
  in_progress_ = true;
  notification_manager_->ShowMigrationProgressNotification();
  // TODO(aidazolic): Upload everything under My files.
  std::move(callback).Run();
}

void LocalFilesMigrationManager::OnMigrationDone() {
  in_progress_ = false;
  if (error_.has_value()) {
    // TODO(aidazolic): Use error message; add on-click action.
    notification_manager_->ShowMigrationErrorNotification(error_.value());
    // TODO(aidazolic): UMA.
    LOG(ERROR) << "Local files migration failed: " << error_.value();
  } else {
    for (auto& observer : observers_) {
      observer.OnMigrationSucceeded();
    }
    // TODO(aidazolic): Pass the path of the folder that files are uploaded to.
    notification_manager_->ShowMigrationCompletedNotification(base::FilePath());
    VLOG(1) << "Local files migration done";
  }
}

}  // namespace policy::local_user_files
