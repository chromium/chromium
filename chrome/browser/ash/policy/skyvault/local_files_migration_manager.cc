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

// Returns true if `cloud_provider` is set to Google Drive or OneDrive.
bool IsMigrationEnabled(CloudProvider cloud_provider) {
  return cloud_provider == CloudProvider::kGoogleDrive ||
         cloud_provider == CloudProvider::kOneDrive;
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

// Generates the destination directory name, combining the "ChromeOS device"
// prefix with a unique identifier of the device.
std::string GenerateDestinationDirName() {
  std::optional<std::string_view> id =
      ash::system::StatisticsProvider::GetInstance()->GetMachineID();
  return std::string(kDestinationDirName) + " " + std::string(id.value_or(""));
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
  PrefService* pref_service = Profile::FromBrowserContext(context_)->GetPrefs();
  state_ = static_cast<State>(
      pref_service->GetInteger(prefs::kSkyVaultMigrationState));

  VLOG(1) << "Loaded migration state: " << StateToString(state_);

  local_user_files_allowed_ = LocalUserFilesAllowed();
  cloud_provider_ = GetMigrationDestination();

  SkyVaultLocalStorageEnabledHistogram(local_user_files_allowed_);

  if (local_user_files_allowed_ || !IsMigrationEnabled(cloud_provider_)) {
    // Migration is now disabled, reset the state.
    if (state_ != State::kUninitialized) {
      LOG(WARNING) << "Migration disabled - resetting the state";
      SetState(State::kUninitialized);
      SkyVaultMigrationResetHistogram(true);
    }
    return;
  }
  // Migration is enabled.
  SkyVaultMigrationEnabledHistogram(cloud_provider_, true);

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
      // TODO(aidazolic): Consider if we should do any special handling.
      for (auto& observer : observers_) {
        observer.OnMigrationSucceeded();
      }
      SetLocalUserFilesWriteEnabled(/*enabled=*/false);
      break;
    case State::kFailure:
      // TODO(b/351971781): Process errors from the error log.
      break;
  }
}

void LocalFilesMigrationManager::Shutdown() {
  weak_factory_.InvalidateWeakPtrs();
}

void LocalFilesMigrationManager::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
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

void LocalFilesMigrationManager::OnLocalUserFilesPolicyChanged() {
  bool local_user_files_allowed_old = local_user_files_allowed_;
  local_user_files_allowed_ = LocalUserFilesAllowed();
  CloudProvider cloud_provider_old = cloud_provider_;
  cloud_provider_ = GetMigrationDestination();

  if (local_user_files_allowed_ == local_user_files_allowed_old &&
      cloud_provider_ == cloud_provider_old) {
    // No change.
    return;
  }

  SkyVaultLocalStorageEnabledHistogram(local_user_files_allowed_);

  // If local files are allowed or migration is turned off, just stop ongoing
  // migration or timers if any.
  if (local_user_files_allowed_ || !IsMigrationEnabled(cloud_provider_)) {
    MaybeStopMigration(cloud_provider_old);
    if (local_user_files_allowed_) {
      SetLocalUserFilesWriteEnabled(/*enabled=*/true);
    }
    return;
  }
  SkyVaultMigrationEnabledHistogram(cloud_provider_, true);

  // If the destination changed, stop ongoing migration or timers if any.
  if (cloud_provider_ != cloud_provider_old) {
    MaybeStopMigration(cloud_provider_old);
  }

  // Check if the destination cloud provider is enabled.
  Profile* profile = Profile::FromBrowserContext(context_);
  const bool google_drive_disabled =
      !drive::DriveIntegrationServiceFactory::FindForProfile(profile)
           ->is_enabled();
  const bool one_drive_disabled =
      !chromeos::cloud_upload::IsMicrosoftOfficeOneDriveIntegrationAllowed(
          profile);
  if ((cloud_provider_ == CloudProvider::kGoogleDrive &&
       google_drive_disabled) ||
      (cloud_provider_ == CloudProvider::kOneDrive && one_drive_disabled)) {
    LOG(WARNING) << "Local files migration policy is set to use "
                 << (cloud_provider_ == CloudProvider::kGoogleDrive
                         ? "Google Drive"
                         : "OneDrive")
                 << ", but it is not enabled for this user.";
    notification_manager_->ShowConfigurationErrorNotification(cloud_provider_);
    SkyVaultMigrationMisconfiguredHistogram(cloud_provider_, true);
    return;
  }

  // Local files are disabled and migration destination is set - initiate
  // migration.
  SetState(State::kPending);
  InformUser();
}

void LocalFilesMigrationManager::InformUser() {
  if (state_ != State::kPending) {
    LOG(ERROR) << "Wrong state when informing the user first time";
    SkyVaultMigrationWrongStateHistogram(
        cloud_provider_, StateErrorContext::kShowDialog, state_);
    return;
  }
  CHECK(!local_user_files_allowed_);
  CHECK(IsMigrationEnabled(cloud_provider_));

  migration_start_time_ = base::Time::Now() + kTotalMigrationTimeout;

  notification_manager_->ShowMigrationInfoDialog(
      cloud_provider_, migration_start_time_,
      base::BindOnce(&LocalFilesMigrationManager::SkipMigrationDelay,
                     weak_factory_.GetWeakPtr()));
  // Schedule another dialog closer to the migration.
  scheduling_timer_->Start(
      FROM_HERE, migration_start_time_ - kFinalMigrationTimeout,
      base::BindOnce(
          &LocalFilesMigrationManager::ScheduleMigrationAndInformUser,
          weak_factory_.GetWeakPtr()));
}

void LocalFilesMigrationManager::ScheduleMigrationAndInformUser() {
  if (local_user_files_allowed_ || !IsMigrationEnabled(cloud_provider_)) {
    return;
  }

  if (state_ != State::kPending) {
    LOG(ERROR) << "Wrong state when informing the user second time";
    SkyVaultMigrationWrongStateHistogram(
        cloud_provider_, StateErrorContext::kShowDialog, state_);
    return;
  }

  notification_manager_->ShowMigrationInfoDialog(
      cloud_provider_, migration_start_time_,
      base::BindOnce(&LocalFilesMigrationManager::SkipMigrationDelay,
                     weak_factory_.GetWeakPtr()));
  // Also schedule migration to automatically start after the timeout.
  scheduling_timer_->Start(
      FROM_HERE, migration_start_time_,
      base::BindOnce(&LocalFilesMigrationManager::OnTimeoutExpired,
                     weak_factory_.GetWeakPtr()));
}

void LocalFilesMigrationManager::SkipMigrationDelay() {
  if (state_ != State::kPending) {
    LOG(ERROR) << "Wrong state in SkipMigrationDelay";
    SkyVaultMigrationWrongStateHistogram(
        cloud_provider_, StateErrorContext::kSkipTimeout, state_);
    return;
  }
  SetState(State::kInProgress);
  scheduling_timer_->Stop();
  GetPathsToUpload();
}

void LocalFilesMigrationManager::OnTimeoutExpired() {
  if (state_ != State::kPending) {
    LOG(ERROR) << "Wrong state in OnTimeoutExpired";
    SkyVaultMigrationWrongStateHistogram(cloud_provider_,
                                         StateErrorContext::kTimeout, state_);
    return;
  }
  // TODO(aidazolic): This could cause issues if the dialog doesn't close fast
  // enough, and the user clicks "Upload now" exactly then.
  SetState(State::kInProgress);
  notification_manager_->CloseDialog();
  GetPathsToUpload();
}

void LocalFilesMigrationManager::GetPathsToUpload() {
  if (state_ != State::kInProgress) {
    LOG(ERROR) << "Wrong state when getting paths to upload";
    SkyVaultMigrationWrongStateHistogram(cloud_provider_,
                                         StateErrorContext::kListFiles, state_);
    return;
  }

  CHECK(!coordinator_->IsRunning());
  // Check policies again.
  if (local_user_files_allowed_ || !IsMigrationEnabled(cloud_provider_)) {
    LOG(ERROR) << "Local files allowed or migration disabled while in "
                  "progress, aborting";
    return;
  }

  Profile* profile = Profile::FromBrowserContext(context_);
  CHECK(profile);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetMyFilesContents, profile),
      base::BindOnce(&LocalFilesMigrationManager::StartMigration,
                     weak_factory_.GetWeakPtr()));
  notification_manager_->ShowMigrationProgressNotification(cloud_provider_);
}

void LocalFilesMigrationManager::StartMigration(
    std::vector<base::FilePath> files) {
  if (state_ != State::kInProgress) {
    LOG(ERROR) << "Wrong state in migration start";
    SkyVaultMigrationWrongStateHistogram(
        cloud_provider_, StateErrorContext::kMigrationStart, state_);
    return;
  }

  CHECK(!coordinator_->IsRunning());
  // Check policies again.
  if (local_user_files_allowed_ || !IsMigrationEnabled(cloud_provider_)) {
    LOG(ERROR) << "Local files allowed or migration disabled while in "
                  "progress, aborting";
    return;
  }

  coordinator_->Run(cloud_provider_, std::move(files),
                    GenerateDestinationDirName(),
                    base::BindOnce(&LocalFilesMigrationManager::OnMigrationDone,
                                   weak_factory_.GetWeakPtr()));
}

void LocalFilesMigrationManager::OnMigrationDone(
    std::map<base::FilePath, MigrationUploadError> errors) {
  if (state_ != State::kInProgress) {
    LOG(ERROR) << "Wrong state in migration done";
    SkyVaultMigrationWrongStateHistogram(
        cloud_provider_, StateErrorContext::kMigrationDone, state_);
    return;
  }

  SkyVaultMigrationFailedHistogram(cloud_provider_, !errors.empty());

  // TODO(b/354709404): Get destination folder path in drive.
  const base::FilePath destination_path = base::FilePath();
  if (!errors.empty()) {
    SetState(State::kFailure);
    LOG(ERROR) << "Local files migration failed.";
    ProcessErrors(std::move(errors));
    return;
  }

  for (auto& observer : observers_) {
    observer.OnMigrationSucceeded();
  }
  notification_manager_->ShowMigrationCompletedNotification(cloud_provider_,
                                                            destination_path);
  VLOG(1) << "Local files migration done";

  SetState(State::kCleanup);
  CleanupLocalFiles();
}

void LocalFilesMigrationManager::ProcessErrors(
    std::map<base::FilePath, MigrationUploadError> errors) {
  CHECK(state_ == State::kFailure);
  CHECK(!errors.empty());
  // TODO(b/354709404): Get destination folder path in drive.
  const base::FilePath destination_path = base::FilePath();
  // TODO(b/351971781): Process retryable errors/show correct message.
  notification_manager_->ShowMigrationErrorNotification(
      cloud_provider_, destination_path, std::move(errors));
}

void LocalFilesMigrationManager::CleanupLocalFiles() {
  if (state_ != State::kCleanup) {
    LOG(ERROR) << "Wrong state in cleanup start";
    SkyVaultMigrationWrongStateHistogram(
        cloud_provider_, StateErrorContext::kCleanupStart, state_);
    return;
  }

  if (cleanup_in_progress_) {
    LOG(ERROR) << "Local files cleanup is already running";
    return;
  }
  cleanup_in_progress_ = true;
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
        cloud_provider_, StateErrorContext::kCleanupDone, state_);
    return;
  }

  cleanup_in_progress_ = false;
  if (error_message.has_value()) {
    LOG(ERROR) << "Local files cleanup failed: " << error_message.value();
  } else {
    VLOG(1) << "Local files cleanup done";
  }
  SetState(State::kCompleted);
  SetLocalUserFilesWriteEnabled(/*enabled=*/false);
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
    CloudProvider previous_provider) {
  // Stop the timer. No-op if not running.
  scheduling_timer_->Stop();

  if (coordinator_->IsRunning()) {
    coordinator_->Stop();
  }

  notification_manager_->CloseAll();
  if (state_ == State::kPending || state_ == State::kInProgress) {
    SkyVaultMigrationStoppedHistogram(previous_provider, true);
  }
  SetState(State::kUninitialized);
}

void LocalFilesMigrationManager::SetState(State new_state) {
  if (state_ == new_state) {
    return;
  }
  state_ = new_state;
  Profile::FromBrowserContext(context_)->GetPrefs()->SetInteger(
      prefs::kSkyVaultMigrationState, static_cast<int>(new_state));
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
