// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/skyvault/histogram_helper.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/files_cleanup_handler.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/error_util.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_context.h"

namespace policy::local_user_files {

namespace {

// Returns true if `destination` is set to Google Drive or OneDrive, or to
// delete local files.
bool IsMigrationEnabled(MigrationDestination destination) {
  return destination != MigrationDestination::kNotSpecified;
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
    if (base::StartsWith(path.BaseName().value(), ".")) {
      continue;
    }
    files.push_back(path);
  }
  return files;
}

// Checks if there are any files that should be uploaded in MyFiles.
bool IsMyFilesEmpty(Profile* profile) {
  base::FilePath my_files_path = GetMyFilesPath(profile);

  base::FileEnumerator enumerator(my_files_path,
                                  /*recursive=*/true,
                                  /*file_type=*/base::FileEnumerator::FILES |
                                      base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (enumerator.GetInfo().IsDirectory()) {
      // Don't count directories as they might be empty.
      continue;
    }
    // Ignore hidden files.
    if (base::StartsWith(path.BaseName().value(), ".")) {
      continue;
    }
    // Found a file.
    return false;
  }
  return true;
}

// Generates a device-unique name for the root folder that all files are
// uploaded to.
std::string GenerateUploadRootName() {
  std::optional<std::string_view> id =
      ash::system::StatisticsProvider::GetInstance()->GetMachineID();
  return std::string(kUploadRootPrefix) + " " + std::string(id.value_or(""));
}

// Converts `state` to its string representation.
std::string StateToString(State state) {
  switch (state) {
    case State::kUninitialized:
      return "uninitialized";
    case State::kPending:
      return "pending";
    case State::kInProgress:
      return "in_progress";
    case State::kCleanup:
      return "clean_up";
    case State::kCompleted:
      return "completed";
    case State::kFailure:
      return "failure";
  }
}

// Records histograms related to SkyVault local storage settings.
void LocalStorageHistograms(Profile* profile, bool local_user_files_allowed) {
  SkyVaultLocalStorageEnabledHistogram(local_user_files_allowed);

  if (local_user_files_allowed) {
    return;  // No further checks needed.
  }

  // If local files are disallowed, check if Downloads are misconfigured.
  FileSaveDestination downloads_destination = GetDownloadsDestination(profile);
  if (downloads_destination == FileSaveDestination::kDownloads ||
      downloads_destination == FileSaveDestination::kNotSpecified) {
    SkyVaultLocalStorageMisconfiguredHistogram(true);
  }
}

// Whether the migration process should end with an error: either the max
// retries are reached, or there are non-retryable errors like running out of
// space on the cloud.
bool ShouldFail(const std::map<base::FilePath, MigrationUploadError> errors,
                int current_retry_count) {
  DCHECK(!errors.empty());

  if (current_retry_count > kMaxRetryCount) {
    return true;
  }

  // Check if there are non-retryable errors.
  for (const auto& error : errors) {
    if (error.second == MigrationUploadError::kCloudQuotaFull) {
      return true;
    }
  }
  return false;
}

// Checks if the destination cloud provider is enabled.
bool IsMigrationMisconfigured(Profile* profile, MigrationDestination provider) {
  switch (provider) {
    case MigrationDestination::kNotSpecified:
      NOTREACHED();
    case MigrationDestination::kGoogleDrive:
      return !drive::DriveIntegrationServiceFactory::FindForProfile(profile)
                  ->is_enabled();
    case MigrationDestination::kOneDrive:
      return !chromeos::cloud_upload::
          IsMicrosoftOfficeOneDriveIntegrationAllowed(profile);
    case MigrationDestination::kDelete:
      // Cannot be misconfigured.
      return false;
  }
}

}  // namespace

LocalFilesMigrationManager::LocalFilesMigrationManager(
    content::BrowserContext* context)
    : context_(context),
      coordinator_(std::make_unique<MigrationCoordinator>(
          Profile::FromBrowserContext(context))),
      scheduling_timer_(std::make_unique<base::WallClockTimer>()) {
  CHECK(base::FeatureList::IsEnabled(features::kSkyVaultV2));

  notification_manager_ =
      MigrationNotificationManagerFactory::GetForBrowserContext(context);
  if (!notification_manager_) {
    CHECK_IS_TEST();
  }
}

LocalFilesMigrationManager::~LocalFilesMigrationManager() = default;

void LocalFilesMigrationManager::Initialize() {
  Profile* profile = Profile::FromBrowserContext(context_);
  PrefService* pref_service = profile->GetPrefs();

  if (pref_service->GetInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_WAITING) {
    pref_service->AddPrefInitObserver(
        base::BindOnce(&LocalFilesMigrationManager::OnPrefsInitialized,
                       weak_factory_.GetWeakPtr()));
  } else {
    InitializeFromPrefs();
  }
}

void LocalFilesMigrationManager::Shutdown() {
  weak_factory_.InvalidateWeakPtrs();
}

void LocalFilesMigrationManager::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);

  if (state_ == State::kCompleted) {
    observer->OnMigrationSucceeded();
  }
}

void LocalFilesMigrationManager::RemoveObserver(Observer* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

void LocalFilesMigrationManager::SetNotificationManagerForTesting(
    MigrationNotificationManager* notification_manager) {
  CHECK_IS_TEST();
  notification_manager_ = notification_manager;
}

void LocalFilesMigrationManager::SetCoordinatorForTesting(
    std::unique_ptr<MigrationCoordinator> coordinator) {
  CHECK_IS_TEST();
  coordinator_ = std::move(coordinator);
}

void LocalFilesMigrationManager::SetCleanupHandlerForTesting(
    base::WeakPtr<chromeos::FilesCleanupHandler> cleanup_handler) {
  CHECK_IS_TEST();
  cleanup_handler_for_testing_ = cleanup_handler;
}

void LocalFilesMigrationManager::OnPrefsInitialized(bool success) {
  if (!success) {
    LOG(ERROR) << "Initializing preferences failed. Migration/deletion will be "
                  "retried in the next session.";
    return;
  }

  InitializeFromPrefs();
}

void LocalFilesMigrationManager::InitializeFromPrefs() {
  Profile* profile = Profile::FromBrowserContext(context_);
  PrefService* pref_service = profile->GetPrefs();
  state_ = static_cast<State>(
      pref_service->GetInteger(prefs::kSkyVaultMigrationState));

  VLOG(1) << "Loaded migration state: " << StateToString(state_);

  current_retry_count_ =
      pref_service->GetInteger(prefs::kSkyVaultMigrationRetryCount);
  VLOG(1) << "Loaded retry count: " << current_retry_count_;
  if (current_retry_count_ > kMaxRetryCount) {
    // Loaded state should be kFailed, but set it explicitly just in case.
    VLOG(1) << "Max retry count reached, setting state to failure";
    SetState(State::kFailure);
  }

  local_user_files_allowed_ = LocalUserFilesAllowed();
  migration_destination_ = GetMigrationDestination();

  // For kDelete, retry cleanup even after kMaxRetryCount failures to ensure
  // policy-enforced deletion. Other destinations treat kFailure as final.
  if (state_ == State::kFailure &&
      migration_destination_ == MigrationDestination::kDelete) {
    current_retry_count_ = 0;
    pref_service->SetInteger(prefs::kSkyVaultMigrationRetryCount,
                             current_retry_count_);
    SetState(State::kCleanup);
  }

  LocalStorageHistograms(profile, local_user_files_allowed_);

  if (local_user_files_allowed_ ||
      !IsMigrationEnabled(migration_destination_)) {
    // Migration is now disabled, reset the state and failure count.
    if (state_ != State::kUninitialized) {
      LOG(WARNING) << "Migration disabled: resetting the state and retry count";
      ResetMigrationPrefs();
      SkyVaultMigrationResetHistogram(true);
    }
    // If migration is not configured, check whether there are already no files
    // to migrate.
    if (!local_user_files_allowed_) {
      DCHECK(!IsMigrationEnabled(migration_destination_));
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(&IsMyFilesEmpty, profile),
          base::BindOnce(&LocalFilesMigrationManager::OnMyFilesChecked,
                         weak_factory_.GetWeakPtr()));
    }
    return;
  }
  // Migration is enabled.
  SkyVaultMigrationEnabledHistogram(migration_destination_, true);

  if (IsMigrationMisconfigured(profile, migration_destination_)) {
    CHECK(IsCloudDestination(migration_destination_));
    LOG(WARNING) << "Local files migration policy is set to use "
                 << (migration_destination_ ==
                             MigrationDestination::kGoogleDrive
                         ? "Google Drive"
                         : "OneDrive")
                 << ", but it is not enabled for this user.";
    SkyVaultMigrationMisconfiguredHistogram(migration_destination_, true);
    if (!notification_manager_) {
      // Can be null in unittests.
      CHECK_IS_TEST();
      return;
    }
    notification_manager_->ShowConfigurationErrorNotification(
        migration_destination_);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&IsMyFilesEmpty, profile),
      base::BindOnce(&LocalFilesMigrationManager::OnMyFilesChecked,
                     weak_factory_.GetWeakPtr()));
}

void LocalFilesMigrationManager::OnLocalUserFilesPolicyChanged() {
  bool local_user_files_allowed_old = local_user_files_allowed_;
  local_user_files_allowed_ = LocalUserFilesAllowed();
  MigrationDestination migration_destination_old = migration_destination_;
  migration_destination_ = GetMigrationDestination();

  if (local_user_files_allowed_ == local_user_files_allowed_old &&
      migration_destination_ == migration_destination_old) {
    // No change.
    return;
  }

  Profile* profile = Profile::FromBrowserContext(context_);

  LocalStorageHistograms(profile, local_user_files_allowed_);

  if (local_user_files_allowed_ ||
      !IsMigrationEnabled(migration_destination_)) {
    MaybeStopMigration(migration_destination_old);
    SkyVaultMigrationResetHistogram(true);
    if (local_user_files_allowed_) {
      SetLocalUserFilesWriteEnabled(/*enabled=*/true);
    } else {
      CHECK(state_ == State::kUninitialized);
      // If migration is not configured, check whether there are already no
      // files to migrate.
      DCHECK(!IsMigrationEnabled(migration_destination_));
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(&IsMyFilesEmpty, profile),
          base::BindOnce(&LocalFilesMigrationManager::OnMyFilesChecked,
                         weak_factory_.GetWeakPtr()));
    }
    return;
  }
  SkyVaultMigrationEnabledHistogram(migration_destination_, true);

  // If the destination changed, stop ongoing migration or timers if any.
  if (migration_destination_ != migration_destination_old &&
      IsMigrationEnabled(migration_destination_old)) {
    // Don't close the dialog as it'll be reshown.
    MaybeStopMigration(
        migration_destination_old, /*close_dialog=*/false,
        base::BindOnce(&LocalFilesMigrationManager::OnMigrationStopped,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  OnMigrationStopped(/*log_file_deleted=*/true);
}

void LocalFilesMigrationManager::OnMigrationStopped(bool log_file_deleted) {
  LOG_IF(ERROR, !log_file_deleted) << "Log file couldn't be deleted";

  if (local_user_files_allowed_ ||
      !IsMigrationEnabled(migration_destination_)) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(context_);
  if (IsMigrationMisconfigured(profile, migration_destination_)) {
    DCHECK(IsCloudDestination(migration_destination_));
    LOG(WARNING) << "Local files migration policy is set to use "
                 << (migration_destination_ ==
                             MigrationDestination::kGoogleDrive
                         ? "Google Drive"
                         : "OneDrive")
                 << ", but it is not enabled for this user.";
    notification_manager_->ShowConfigurationErrorNotification(
        migration_destination_);
    SkyVaultMigrationMisconfiguredHistogram(migration_destination_, true);
    return;
  }

  // Local files are disabled and migration destination is set - initiate
  // migration if there are any files to upload.
  SetState(State::kPending);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&IsMyFilesEmpty, profile),
      base::BindOnce(&LocalFilesMigrationManager::OnMyFilesChecked,
                     weak_factory_.GetWeakPtr()));
}

void LocalFilesMigrationManager::OnMyFilesChecked(bool is_empty) {
  if (local_user_files_allowed_) {
    return;
  }
  if (!IsMigrationEnabled(migration_destination_)) {
    // If migration is not configured, but no files - proceed to clean up.
    if (is_empty) {
      // Notify to unmount local folder in volume manager.
      NotifySuccess();
      SetState(State::kCleanup);
      CleanupLocalFiles();
    }
    return;
  }

  if (is_empty) {
    // Completed state is handled below. For any other state, notify
    // observers and also cleanup empty folders.
    if (state_ != State::kCompleted) {
      NotifySuccess();
      SetState(State::kCleanup);
    }
  }

  switch (state_) {
    case State::kUninitialized:
    case State::kPending:
      SetState(State::kPending);
      InformUser();
      break;
    case State::kInProgress:
      GetPathsToUpload();
      break;
    case State::kCleanup:
      CleanupLocalFiles();
      break;
    case State::kCompleted:
      NotifySuccess();
      SetLocalUserFilesWriteEnabled(/*enabled=*/false);
      break;
    case State::kFailure:
      break;
  }
}

void LocalFilesMigrationManager::InformUser() {
  if (state_ != State::kPending) {
    LOG(ERROR) << "Wrong state when informing the user first time";
    SkyVaultMigrationWrongStateHistogram(
        migration_destination_, StateErrorContext::kShowDialog, state_);
    return;
  }
  CHECK(!local_user_files_allowed_);
  CHECK(IsMigrationEnabled(migration_destination_));

  const base::Time now = base::Time::Now();
  base::Time scheduled_start_time = now + kTotalMigrationTimeout;
  if (base::FeatureList::IsEnabled(features::kSkyVaultV3)) {
    PrefService* pref_service =
        Profile::FromBrowserContext(context_)->GetPrefs();
    scheduled_start_time =
        pref_service->GetTime(prefs::kSkyVaultMigrationScheduledStartTime);
    if (scheduled_start_time.is_null()) {
      scheduled_start_time = now + kTotalMigrationTimeout;
      pref_service->SetTime(prefs::kSkyVaultMigrationScheduledStartTime,
                            scheduled_start_time);
    }
  }

  const base::TimeDelta remaining_time = scheduled_start_time - now;
  if (remaining_time.is_negative()) {
    SkyVaultMigrationScheduledTimeInPastInformUser(migration_destination_,
                                                   true);
    OnTimeoutExpired();
    return;
  }
  if (remaining_time <= kFinalMigrationTimeout) {
    ScheduleMigrationAndInformUser(scheduled_start_time);
    return;
  }

  notification_manager_->ShowMigrationInfoDialog(
      migration_destination_, scheduled_start_time,
      base::BindOnce(&LocalFilesMigrationManager::SkipMigrationDelay,
                     weak_factory_.GetWeakPtr()));
  // Schedule another dialog closer to the migration.
  scheduling_timer_->Start(
      FROM_HERE, scheduled_start_time - kFinalMigrationTimeout,
      base::BindOnce(
          &LocalFilesMigrationManager::ScheduleMigrationAndInformUser,
          weak_factory_.GetWeakPtr(), scheduled_start_time));
}

void LocalFilesMigrationManager::ScheduleMigrationAndInformUser(
    const base::Time scheduled_start_time) {
  if (local_user_files_allowed_ ||
      !IsMigrationEnabled(migration_destination_)) {
    return;
  }

  if (state_ != State::kPending) {
    LOG(ERROR) << "Wrong state when informing the user second time";
    SkyVaultMigrationWrongStateHistogram(
        migration_destination_, StateErrorContext::kShowDialog, state_);
    return;
  }

  const base::TimeDelta remaining_time =
      scheduled_start_time - base::Time::Now();
  if (remaining_time.is_negative()) {
    LOG(ERROR) << "Scheduled migration time already passed in "
                  "ScheduleMigrationAndInformUser(), starting immediately.";
    SkyVaultMigrationScheduledTimeInPastScheduleMigration(
        migration_destination_, true);
    OnTimeoutExpired();
    return;
  }

  notification_manager_->ShowMigrationInfoDialog(
      migration_destination_, scheduled_start_time,
      base::BindOnce(&LocalFilesMigrationManager::SkipMigrationDelay,
                     weak_factory_.GetWeakPtr()));
  // Also schedule migration to automatically start after the timeout.
  scheduling_timer_->Start(
      FROM_HERE, scheduled_start_time,
      base::BindOnce(&LocalFilesMigrationManager::OnTimeoutExpired,
                     weak_factory_.GetWeakPtr()));
}

void LocalFilesMigrationManager::SkipMigrationDelay() {
  if (state_ != State::kPending) {
    LOG(ERROR) << "Wrong state in SkipMigrationDelay";
    SkyVaultMigrationWrongStateHistogram(
        migration_destination_, StateErrorContext::kSkipTimeout, state_);
    return;
  }
  scheduling_timer_->Stop();
  if (migration_destination_ == MigrationDestination::kDelete) {
    SetState(State::kCleanup);
    CleanupLocalFiles();
    return;
  }
  SetState(State::kInProgress);
  GetPathsToUpload();
}

void LocalFilesMigrationManager::OnTimeoutExpired() {
  if (state_ != State::kPending) {
    LOG(ERROR) << "Wrong state in OnTimeoutExpired";
    SkyVaultMigrationWrongStateHistogram(migration_destination_,
                                         StateErrorContext::kTimeout, state_);
    return;
  }
  notification_manager_->CloseDialog();
  if (migration_destination_ == MigrationDestination::kDelete) {
    SetState(State::kCleanup);
    CleanupLocalFiles();
    return;
  }
  SetState(State::kInProgress);
  GetPathsToUpload();
}

void LocalFilesMigrationManager::GetPathsToUpload() {
  if (state_ != State::kInProgress) {
    LOG(ERROR) << "Wrong state when getting paths to upload";
    SkyVaultMigrationWrongStateHistogram(migration_destination_,
                                         StateErrorContext::kListFiles, state_);
    return;
  }

  CHECK(!coordinator_->IsRunning());
  // Check policies again.
  if (local_user_files_allowed_ ||
      !IsMigrationEnabled(migration_destination_)) {
    LOG(ERROR) << "Local files allowed or migration disabled while in "
                  "progress, aborting";
    return;
  }
  if (migration_destination_ == MigrationDestination::kDelete) {
    // Although unlikely, it could happen we reach this function for the delete
    // case if the state was loaded from the device and the policy changed
    // between two sessions. Rather than fail, skip ahead to cleanup.
    LOG(ERROR) << "Reached GetPathsToUpload() but the migration destination is "
                  "delete, skipping to cleanup";
    SetState(State::kCleanup);
    CleanupLocalFiles();
    return;
  }

  Profile* profile = Profile::FromBrowserContext(context_);
  CHECK(profile);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetMyFilesContents, profile),
      base::BindOnce(&LocalFilesMigrationManager::StartMigration,
                     weak_factory_.GetWeakPtr()));
  notification_manager_->ShowMigrationProgressNotification(
      migration_destination_);
}

void LocalFilesMigrationManager::StartMigration(
    std::vector<base::FilePath> files) {
  if (state_ != State::kInProgress) {
    LOG(ERROR) << "Wrong state in migration start";
    SkyVaultMigrationWrongStateHistogram(
        migration_destination_, StateErrorContext::kMigrationStart, state_);
    return;
  }

  CHECK(!coordinator_->IsRunning());
  // Check policies again.
  if (local_user_files_allowed_ ||
      !IsMigrationEnabled(migration_destination_)) {
    LOG(ERROR) << "Local files allowed or migration disabled while in "
                  "progress, aborting";
    return;
  }
  if (migration_destination_ == MigrationDestination::kDelete) {
    LOG(ERROR) << "Reached StartMigration() but the migration destination is "
                  "delete, skipping to cleanup";
    SetState(State::kCleanup);
    CleanupLocalFiles();
    return;
  }
  DCHECK(IsCloudDestination(migration_destination_));

  PrefService* pref_service = Profile::FromBrowserContext(context_)->GetPrefs();
  const base::Time start_time =
      pref_service->GetTime(prefs::kSkyVaultMigrationStartTime);
  if (start_time.is_null()) {
    pref_service->SetTime(prefs::kSkyVaultMigrationStartTime,
                          base::Time::Now());
  }

  upload_root_ = GenerateUploadRootName();
  coordinator_->Run(migration_destination_, std::move(files), upload_root_,
                    base::BindOnce(&LocalFilesMigrationManager::OnMigrationDone,
                                   weak_factory_.GetWeakPtr()));
}

void LocalFilesMigrationManager::OnMigrationDone(
    std::map<base::FilePath, MigrationUploadError> errors,
    base::FilePath upload_root_path,
    base::FilePath error_log_path) {
  if (state_ != State::kInProgress) {
    LOG(ERROR) << "Wrong state in migration done";
    SkyVaultMigrationWrongStateHistogram(
        migration_destination_, StateErrorContext::kMigrationDone, state_);
    return;
  }
  DCHECK(IsCloudDestination(migration_destination_));

  const base::Time start_time =
      Profile::FromBrowserContext(context_)->GetPrefs()->GetTime(
          prefs::kSkyVaultMigrationStartTime);
  const base::TimeDelta duration = base::Time::Now() - start_time;

  if (errors.empty()) {
    NotifySuccess();
    notification_manager_->ShowMigrationCompletedNotification(
        migration_destination_, upload_root_path);
    VLOG(1) << "Local files migration done";
    SkyVaultMigrationDoneHistograms(migration_destination_, /*success=*/true,
                                    duration);
    SetState(State::kCleanup);
    CleanupLocalFiles();
    return;
  }

  bool failed = ShouldFail(errors, ++current_retry_count_);
  Profile::FromBrowserContext(context_)->GetPrefs()->SetInteger(
      prefs::kSkyVaultMigrationRetryCount, current_retry_count_);

  if (failed) {
    SkyVaultMigrationDoneHistograms(migration_destination_, /*success=*/false,
                                    duration);
    SetState(State::kFailure);
    LOG(ERROR) << "Local files migration failed.";
    ProcessErrors(std::move(errors), error_log_path);
    return;
  }
  // Retry
  SkyVaultMigrationRetryHistogram(current_retry_count_);
  SetState(State::kInProgress);
  GetPathsToUpload();
}

void LocalFilesMigrationManager::ProcessErrors(
    std::map<base::FilePath, MigrationUploadError> errors,
    base::FilePath error_log_path) {
  CHECK(state_ == State::kFailure);
  CHECK(!errors.empty());
  DCHECK(IsCloudDestination(migration_destination_));
  notification_manager_->ShowMigrationErrorNotification(
      migration_destination_, upload_root_, error_log_path);
}

void LocalFilesMigrationManager::CleanupLocalFiles() {
  if (state_ != State::kCleanup) {
    LOG(ERROR) << "Wrong state in cleanup start";
    SkyVaultMigrationWrongStateHistogram(
        migration_destination_, StateErrorContext::kCleanupStart, state_);
    return;
  }

  if (cleanup_in_progress_) {
    LOG(ERROR) << "Local files cleanup is already running";
    return;
  }
  cleanup_in_progress_ = true;
  if (cleanup_handler_for_testing_) {
    CHECK_IS_TEST();
    cleanup_handler_for_testing_->Cleanup(base::BindOnce(
        &LocalFilesMigrationManager::OnCleanupDone, weak_factory_.GetWeakPtr(),
        /*cleanup_handler=*/nullptr));
    return;
  }
  std::unique_ptr<chromeos::FilesCleanupHandler> cleanup_handler =
      std::make_unique<chromeos::FilesCleanupHandler>();
  chromeos::FilesCleanupHandler* cleanup_handler_ptr = cleanup_handler.get();
  cleanup_handler_ptr->Cleanup(
      base::BindOnce(&LocalFilesMigrationManager::OnCleanupDone,
                     weak_factory_.GetWeakPtr(), std::move(cleanup_handler)));
}

void LocalFilesMigrationManager::OnCleanupDone(
    std::unique_ptr<chromeos::FilesCleanupHandler> cleanup_handler,
    const std::optional<std::string>& error_message) {
  if (state_ != State::kCleanup) {
    LOG(ERROR) << "Wrong state in cleanup done";
    SkyVaultMigrationWrongStateHistogram(
        migration_destination_, StateErrorContext::kCleanupDone, state_);
    return;
  }

  cleanup_in_progress_ = false;
  const bool cleanup_failed = error_message.has_value();

  // Cleanup is called even if migration destination isn't specified if there
  // are no local files, but skip recording in that case.
  if (migration_destination_ != MigrationDestination::kNotSpecified) {
    SkyVaultMigrationCleanupErrorHistogram(migration_destination_,
                                           cleanup_failed);
  }

  if (cleanup_failed) {
    LOG(ERROR) << "Local files cleanup failed: " << error_message.value();

    bool failed_too_many_times = ++current_retry_count_ > kMaxRetryCount;
    Profile::FromBrowserContext(context_)->GetPrefs()->SetInteger(
        prefs::kSkyVaultMigrationRetryCount, current_retry_count_);
    if (failed_too_many_times) {
      SkyVaultDeletionDoneHistogram(/*success=*/false);
      SetState(State::kFailure);
      LOG(ERROR) << "Local files cleanup failed too many times.";
      return;
    }
    // Retry cleanup if deletion is enforced by policy.
    if (migration_destination_ == MigrationDestination::kDelete) {
      SkyVaultDeletionRetryHistogram(current_retry_count_);
      SetState(State::kCleanup);
      CleanupLocalFiles();
      return;
    }
  } else {
    VLOG(1) << "Local files cleanup done";
    // Notify success and show notification after successful deletion if it's
    // enforced by policy.
    if (migration_destination_ == MigrationDestination::kDelete) {
      SkyVaultDeletionDoneHistogram(/*success=*/true);
      NotifySuccess();
      notification_manager_->ShowDeletionCompletedNotification();
    }
  }

  SetLocalUserFilesWriteEnabled(/*enabled=*/false);
  SetState(State::kCompleted);
}

void LocalFilesMigrationManager::SetLocalUserFilesWriteEnabled(bool enabled) {
  const user_manager::User* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(context_);
  user_data_auth::SetUserDataStorageWriteEnabledRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(user->GetAccountId());
  request.set_enabled(enabled);
  ash::UserDataAuthClient::Get()->SetUserDataStorageWriteEnabled(
      request,
      base::BindOnce(&LocalFilesMigrationManager::OnFilesWriteRestricted,
                     weak_factory_.GetWeakPtr()));
}

void LocalFilesMigrationManager::OnFilesWriteRestricted(
    std::optional<user_data_auth::SetUserDataStorageWriteEnabledReply> reply) {
  bool failed = !reply.has_value() ||
                reply->error() != user_data_auth::CRYPTOHOME_ERROR_NOT_SET;
  if (failed) {
    LOG(ERROR) << "Could not restrict write access";
  }
  SkyVaultMigrationWriteAccessErrorHistogram(failed);
}

void LocalFilesMigrationManager::MaybeStopMigration(
    MigrationDestination previous_provider,
    bool close_dialog,
    MigrationStoppedCallback on_stopped_cb) {
  // Stop the timer. No-op if not running.
  scheduling_timer_->Stop();

  coordinator_->Cancel(std::move(on_stopped_cb));

  notification_manager_->CloseNotifications();
  if (close_dialog) {
    notification_manager_->CloseDialog();
  }
  if (state_ == State::kPending || state_ == State::kInProgress) {
    SkyVaultMigrationStoppedHistogram(previous_provider, true);
  }
  ResetMigrationPrefs();
  NotifyReset();
}

void LocalFilesMigrationManager::SetState(State new_state) {
  if (state_ == new_state) {
    return;
  }
  state_ = new_state;
  Profile::FromBrowserContext(context_)->GetPrefs()->SetInteger(
      prefs::kSkyVaultMigrationState, static_cast<int>(new_state));
}

void LocalFilesMigrationManager::ResetMigrationPrefs() {
  SetState(State::kUninitialized);
  current_retry_count_ = 0;
  PrefService* pref_service = Profile::FromBrowserContext(context_)->GetPrefs();
  pref_service->SetInteger(prefs::kSkyVaultMigrationRetryCount,
                           current_retry_count_);
  pref_service->SetTime(prefs::kSkyVaultMigrationStartTime, base::Time());
  pref_service->SetTime(prefs::kSkyVaultMigrationScheduledStartTime,
                        base::Time());
}

void LocalFilesMigrationManager::NotifySuccess() {
  for (auto& observer : observers_) {
    observer.OnMigrationSucceeded();
  }
}

void LocalFilesMigrationManager::NotifyReset() {
  for (auto& observer : observers_) {
    observer.OnMigrationReset();
  }
}

// static
LocalFilesMigrationManagerFactory*
LocalFilesMigrationManagerFactory::GetInstance() {
  static base::NoDestructor<LocalFilesMigrationManagerFactory> factory;
  return factory.get();
}

LocalFilesMigrationManager*
LocalFilesMigrationManagerFactory::GetForBrowserContext(
    content::BrowserContext* context,
    bool create) {
  return static_cast<LocalFilesMigrationManager*>(
      GetInstance()->GetServiceForBrowserContext(context, create));
}

LocalFilesMigrationManagerFactory::LocalFilesMigrationManagerFactory()
    : ProfileKeyedServiceFactory(
          "LocalFilesMigrationManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(policy::local_user_files::MigrationNotificationManagerFactory::
                GetInstance());
}

LocalFilesMigrationManagerFactory::~LocalFilesMigrationManagerFactory() =
    default;

bool LocalFilesMigrationManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
LocalFilesMigrationManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kSkyVaultV2)) {
    return nullptr;
  }

  std::unique_ptr<LocalFilesMigrationManager> instance =
      std::make_unique<LocalFilesMigrationManager>(context);
  instance->Initialize();
  return instance;
}

}  // namespace policy::local_user_files
