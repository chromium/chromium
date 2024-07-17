// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"

#include <memory>
#include <string>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"
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

// Delay the migration for a total of 24 hours.
const base::TimeDelta kTotalMigrationTimeout = base::Hours(24);

// Show another dialog 1 hour before the migration.
const base::TimeDelta kRemainingMigrationTimeout = base::Hours(1);

// The prefix of the directory the files should be uploaded to. Used with the
// unique identifier of the device to form the directory's full name.
constexpr char kDestinationDirName[] = "ChromeOS device";

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

// Returns a list of files under MyFiles.
std::vector<base::FilePath> GetMyFilesContents(Profile* profile) {
  base::FilePath my_files_path = GetMyFilesPath(profile);
  std::vector<base::FilePath> files;

  base::FileEnumerator enumerator(my_files_path,
                                  /*recursive=*/true,
                                  /*file_type=*/base::FileEnumerator::FILES |
                                      base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (enumerator.GetInfo().IsDirectory()) {
      // Do not move directories - this moves the contents too.
      continue;
    }
    // Ignore hidden files.
    // TODO(aidazolic): Also Play and Linux?
    if (base::StartsWith(path.BaseName().value(), ".")) {
      continue;
    }
    files.push_back(path);
  }
  return files;
}

}  // namespace

// static
LocalFilesMigrationManager
LocalFilesMigrationManager::CreateLocalFilesMigrationManagerForTesting(
    content::BrowserContext* context,
    std::unique_ptr<MigrationNotificationManager> notification_manager,
    std::unique_ptr<MigrationCoordinator> coordinator) {
  CHECK_IS_TEST();
  return LocalFilesMigrationManager(context, std::move(notification_manager),
                                    std::move(coordinator));
}

LocalFilesMigrationManager::LocalFilesMigrationManager(
    content::BrowserContext* context)
    : LocalFilesMigrationManager(context,
                                 std::make_unique<MigrationNotificationManager>(
                                     Profile::FromBrowserContext(context)),
                                 std::make_unique<MigrationCoordinator>(
                                     Profile::FromBrowserContext(context))) {}

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

LocalFilesMigrationManager::LocalFilesMigrationManager(
    content::BrowserContext* context,
    std::unique_ptr<MigrationNotificationManager> notification_manager,
    std::unique_ptr<MigrationCoordinator> coordinator)
    : context_(context),
      notification_manager_(std::move(notification_manager)),
      coordinator_(std::move(coordinator)),
      scheduling_timer_(std::make_unique<base::WallClockTimer>()) {
  CHECK(base::FeatureList::IsEnabled(features::kSkyVaultV2));

  pref_change_registrar_.Init(g_browser_process->local_state());
  pref_change_registrar_.Add(
      prefs::kLocalUserFilesMigrationDestination,
      base::BindRepeating(
          &LocalFilesMigrationManager::OnLocalUserFilesPolicyChanged,
          base::Unretained(this)));
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
  // migration or timers if any.
  if (local_user_files_allowed_ || !IsMigrationEnabled(cloud_provider_)) {
    MaybeStopMigration();
    return;
  }

  // If the destination changed, stop ongoing migration or timers if any.
  if (IsMigrationEnabled(cloud_provider_) &&
      cloud_provider_ != cloud_provider_old) {
    MaybeStopMigration();
  }

  // Local files are disabled and migration destination is set - initiate
  // migration.
  InformUser();
}

void LocalFilesMigrationManager::InformUser() {
  CHECK(!local_user_files_allowed_);
  CHECK(IsMigrationEnabled(cloud_provider_));

  notification_manager_->ShowMigrationInfoDialog(
      cloud_provider_, kTotalMigrationTimeout,
      base::BindOnce(&LocalFilesMigrationManager::SkipMigrationDelay,
                     weak_factory_.GetWeakPtr()));
  // Schedule another dialog closer to the migration.
  scheduling_timer_->Start(
      FROM_HERE,
      base::Time::Now() + (kTotalMigrationTimeout - kRemainingMigrationTimeout),
      base::BindOnce(
          &LocalFilesMigrationManager::ScheduleMigrationAndInformUser,
          weak_factory_.GetWeakPtr()));
}

void LocalFilesMigrationManager::ScheduleMigrationAndInformUser() {
  if (local_user_files_allowed_ || !IsMigrationEnabled(cloud_provider_)) {
    return;
  }

  notification_manager_->ShowMigrationInfoDialog(
      cloud_provider_, kRemainingMigrationTimeout,
      base::BindOnce(&LocalFilesMigrationManager::SkipMigrationDelay,
                     weak_factory_.GetWeakPtr()));
  // Also schedule migration to automatically start after the timeout.
  scheduling_timer_->Start(
      FROM_HERE, base::Time::Now() + kRemainingMigrationTimeout,
      base::BindOnce(&LocalFilesMigrationManager::OnTimeoutExpired,
                     weak_factory_.GetWeakPtr()));
}

void LocalFilesMigrationManager::SkipMigrationDelay() {
  scheduling_timer_->Stop();
  GetPathsToUpload();
}

void LocalFilesMigrationManager::OnTimeoutExpired() {
  // TODO(aidazolic): This could cause issues if the dialog doesn't close fast
  // enough, and the user clicks "Upload now" exactly then.
  notification_manager_->CloseDialog();
  GetPathsToUpload();
}

void LocalFilesMigrationManager::GetPathsToUpload() {
  CHECK(!coordinator_->IsRunning());
  // Check policies again.
  if (local_user_files_allowed_ || !IsMigrationEnabled(cloud_provider_)) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(context_);
  CHECK(profile);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetMyFilesContents, profile),
      base::BindOnce(&LocalFilesMigrationManager::StartMigration,
                     weak_factory_.GetWeakPtr()));
  in_progress_ = true;
  notification_manager_->ShowMigrationProgressNotification(cloud_provider_);
}

void LocalFilesMigrationManager::StartMigration(
    std::vector<base::FilePath> files) {
  CHECK(!coordinator_->IsRunning());
  // Check policies again.
  if (local_user_files_allowed_ || !IsMigrationEnabled(cloud_provider_)) {
    return;
  }

  // TODO(aidazolic): Add unique ID of the device.
  coordinator_->Run(cloud_provider_, std::move(files), kDestinationDirName,
                    base::BindOnce(&LocalFilesMigrationManager::OnMigrationDone,
                                   weak_factory_.GetWeakPtr()));
}

void LocalFilesMigrationManager::OnMigrationDone(
    std::map<base::FilePath, MigrationUploadError> errors) {
  in_progress_ = false;
  if (!errors.empty()) {
    // TODO(aidazolic): Use error message; add on-click action.
    notification_manager_->ShowMigrationErrorNotification(cloud_provider_,
                                                          std::move(errors));

    LOG(ERROR) << "Local files migration failed.";
  } else {
    for (auto& observer : observers_) {
      observer.OnMigrationSucceeded();
    }
    notification_manager_->ShowMigrationCompletedNotification(cloud_provider_,
                                                              base::FilePath());
    VLOG(1) << "Local files migration done";
  }
}

void LocalFilesMigrationManager::MaybeStopMigration() {
  // Stop the timer. No-op if not running.
  scheduling_timer_->Stop();

  if (coordinator_->IsRunning()) {
    coordinator_->Stop();
  }

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
    : ProfileKeyedServiceFactory(
          "LocalFilesMigrationManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

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
