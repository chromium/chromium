// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace policy::local_user_files {

namespace {

// Delay the migration for 24 hours.
const base::TimeDelta kMigrationTimeout = base::Hours(24);

// Returns true if `cloud_provider` is set to Google Drive or OneDrive.
bool IsMigrationEnabled(CloudProvider cloud_provider) {
  return cloud_provider == CloudProvider::kGoogleDrive ||
         cloud_provider == CloudProvider::kOneDrive;
}

// Converts `destination`, which should hold the value of the
// `kLocalUserFilesMigrationDestination` pref, to the CloudProvider enum value.
CloudProvider StringToCloudProvider(const std::string destination) {
  if (destination == download_dir_util::kLocationGoogleDrive) {
    return CloudProvider::kGoogleDrive;
  }
  if (destination == download_dir_util::kLocationOneDrive) {
    return CloudProvider::kOneDrive;
  }
  if (destination == "read_only") {
    return CloudProvider::kNotSpecified;
  }
  LOG(ERROR) << "Unexpected destination value " << destination;
  return CloudProvider::kNotSpecified;
}

}  // namespace

LocalFilesMigrationManager::LocalFilesMigrationManager(
    content::BrowserContext* context)
    : context_(context),
      notification_manager_(std::make_unique<MigrationNotificationManager>(
          Profile::FromBrowserContext(context))),
      start_delay_timer_(std::make_unique<base::WallClockTimer>()) {
  CHECK(base::FeatureList::IsEnabled(features::kSkyVaultV2));

  pref_change_registrar_.Init(g_browser_process->local_state());
  pref_change_registrar_.Add(
      prefs::kLocalUserFilesMigrationDestination,
      base::BindRepeating(
          &LocalFilesMigrationManager::OnLocalUserFilesPolicyChanged,
          base::Unretained(this)));
}

LocalFilesMigrationManager::~LocalFilesMigrationManager() {
  pref_change_registrar_.RemoveAll();
}

void LocalFilesMigrationManager::Shutdown() {
  weak_factory_.InvalidateWeakPtrs();

  notification_manager_.reset();
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
  bool local_user_files_allowed_old = local_user_files_allowed_;
  local_user_files_allowed_ = LocalUserFilesAllowed();
  std::string destination = g_browser_process->local_state()->GetString(
      prefs::kLocalUserFilesMigrationDestination);
  CloudProvider cloud_provider_old = cloud_provider_;
  cloud_provider_ = StringToCloudProvider(destination);

  if (local_user_files_allowed_ == local_user_files_allowed_old &&
      cloud_provider_ == cloud_provider_old) {
    // No change.
    return;
  }

  // If local files are allowed or migration is turned off, just stop ongoing
  // migration if any.
  if (local_user_files_allowed_ || !IsMigrationEnabled(cloud_provider_)) {
    MaybeStopMigration();
    return;
  }

  // If the destination changed, stop ongoing migration if any.
  if (cloud_provider_ != cloud_provider_old) {
    MaybeStopMigration();
  }

  // Local files are disabled and migration destination is set - initiate
  // migration.
  InitiateMigration();
}

void LocalFilesMigrationManager::InitiateMigration() {
  CHECK(!local_user_files_allowed_);
  CHECK(IsMigrationEnabled(cloud_provider_));

  notification_manager_->ShowMigrationInfoDialog(
      cloud_provider_, kMigrationTimeout,
      base::BindOnce(&LocalFilesMigrationManager::SkipMigrationDelay,
                     weak_factory_.GetWeakPtr()));
  start_delay_timer_->Start(
      FROM_HERE, base::Time::Now() + kMigrationTimeout,
      base::BindOnce(&LocalFilesMigrationManager::StartMigration,
                     weak_factory_.GetWeakPtr()));
}

void LocalFilesMigrationManager::SkipMigrationDelay() {
  if (start_delay_timer_->IsRunning()) {
    start_delay_timer_->Stop();
  }
  StartMigration();
}

void LocalFilesMigrationManager::StartMigration() {
  in_progress_ = true;
  notification_manager_->ShowMigrationProgressNotification(cloud_provider_);
  // TODO(aidazolic): Upload everything under My files.
  OnMigrationDone();
}

void LocalFilesMigrationManager::OnMigrationDone() {
  in_progress_ = false;
  if (error_.has_value()) {
    // TODO(aidazolic): Use error message; add on-click action.
    notification_manager_->ShowMigrationErrorNotification(cloud_provider_,
                                                          error_.value());
    // TODO(aidazolic): UMA.
    LOG(ERROR) << "Local files migration failed: " << error_.value();
  } else {
    for (auto& observer : observers_) {
      observer.OnMigrationSucceeded();
    }
    // TODO(aidazolic): Pass the path of the folder that files are uploaded to.
    notification_manager_->ShowMigrationCompletedNotification(cloud_provider_,
                                                              base::FilePath());
    VLOG(1) << "Local files migration done";
  }
}

void LocalFilesMigrationManager::MaybeStopMigration() {
  // TODO(aidazolic): Implementation.
  if (in_progress_) {
    in_progress_ = false;
  }
  notification_manager_->CloseAll();
}

// static
LocalFilesMigrationManagerFactory*
LocalFilesMigrationManagerFactory::GetInstance() {
  static base::NoDestructor<LocalFilesMigrationManagerFactory> factory;
  return factory.get();
}

LocalFilesMigrationManager*
LocalFilesMigrationManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LocalFilesMigrationManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

LocalFilesMigrationManagerFactory::LocalFilesMigrationManagerFactory()
    : ProfileKeyedServiceFactory("LocalFilesMigrationManager",
                                 ProfileSelections::BuildForRegularProfile()) {}

LocalFilesMigrationManagerFactory::~LocalFilesMigrationManagerFactory() =
    default;

bool LocalFilesMigrationManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
LocalFilesMigrationManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<LocalFilesMigrationManager>(context);
}

}  // namespace policy::local_user_files
