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
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/policy/skyvault/local_user_files_policy_observer.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace policy::local_user_files {

LocalFilesMigrationManager::LocalFilesMigrationManager()
    : notification_manager_(std::make_unique<MigrationNotificationManager>()) {}

LocalFilesMigrationManager::~LocalFilesMigrationManager() = default;

void LocalFilesMigrationManager::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void LocalFilesMigrationManager::RemoveObserver(Observer* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

void LocalFilesMigrationManager::OnLocalUserFilesPolicyChanged() {
  // TODO(aidazolic): Do not start migration immediately. When local files are
  // disabled, notify the user and trigger migration either 24 hours later or
  // upon the next system reboot.
  // TODO(aidazolic): Stop ongoing migration if the policy resets to allow local
  // files?
  MaybeMigrateFiles(base::BindOnce(&LocalFilesMigrationManager::OnMigrationDone,
                                   weak_factory_.GetWeakPtr()));
}

bool LocalFilesMigrationManager::ShouldStart() {
  if (!base::FeatureList::IsEnabled(features::kSkyVaultV2) || in_progress_) {
    return false;
  }
  if (LocalUserFilesAllowed()) {
    return false;
  }
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  CHECK(profile);
  const PrefService* const prefs = profile->GetPrefs();
  CHECK(prefs);
  const std::string defaultLocation =
      prefs->GetString(prefs::kFilesAppDefaultLocation);
  const bool download_directory_set =
      defaultLocation == download_dir_util::kLocationGoogleDrive ||
      defaultLocation == download_dir_util::kLocationOneDrive;
  // Remove this when we can use the new policy?
  if (!download_directory_set) {
    // SkyVault is misconfigured.
    // TODO(aidazolic): Show an error notification if there are any files.
    return false;
  }
  return true;  // Migration should start only if all conditions are met.
}

void LocalFilesMigrationManager::MaybeMigrateFiles(
    base::OnceCallback<void()> callback) {
  if (!ShouldStart()) {
    return;
  }
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
