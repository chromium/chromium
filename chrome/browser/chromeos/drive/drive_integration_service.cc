// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_integration_service.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/drivefs_native_message_host.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/drivefs/drivefs_bootstrap.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/drive_notification_manager.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/event_logger.h"
#include "components/drive/resource_metadata_storage.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/user_agent.h"
#include "google_apis/drive/auth_service.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

using content::BrowserContext;
using content::BrowserThread;

namespace drive {
namespace {

// Name of the directory used to store metadata.
const base::FilePath::CharType kMetadataDirectory[] = FILE_PATH_LITERAL("meta");

// Name of the directory used to store cached files.
const base::FilePath::CharType kCacheFileDirectory[] =
    FILE_PATH_LITERAL("files");

// Name of the directory used to store temporary files.
const base::FilePath::CharType kTemporaryFileDirectory[] =
    FILE_PATH_LITERAL("tmp");

void DeleteDirectoryContents(const base::FilePath& dir) {
  base::FileEnumerator content_enumerator(
      dir, false, base::FileEnumerator::FILES |
                      base::FileEnumerator::DIRECTORIES |
                      base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath path = content_enumerator.Next(); !path.empty();
       path = content_enumerator.Next()) {
    base::DeletePathRecursively(path);
  }
}

base::FilePath FindUniquePath(const base::FilePath& base_name) {
  auto target = base_name;
  for (int uniquifier = 1; base::PathExists(target); ++uniquifier) {
    target = base_name.InsertBeforeExtensionASCII(
        base::StringPrintf(" (%d)", uniquifier));
  }
  return target;
}

base::FilePath GetRecoveredFilesPath(
    const base::FilePath& downloads_directory) {
  const std::string& dest_directory_name = l10n_util::GetStringUTF8(
      IDS_FILE_BROWSER_RECOVERED_FILES_FROM_GOOGLE_DRIVE_DIRECTORY_NAME);
  return FindUniquePath(downloads_directory.Append(dest_directory_name));
}

// Initializes FileCache and ResourceMetadata.
// Must be run on the same task runner used by |cache| and |resource_metadata|.
FileError InitializeMetadata(
    const base::FilePath& cache_root_directory,
    internal::ResourceMetadataStorage* metadata_storage,
    const base::FilePath& downloads_directory) {
  if (!base::DirectoryExists(
          cache_root_directory.Append(kTemporaryFileDirectory))) {
    if (base::SysInfo::IsRunningOnChromeOS())
      LOG(ERROR) << "/tmp should have been created as clear.";
    // Create /tmp directory as encrypted. Cryptohome will re-create /tmp
    // direcotry at the next login.
    if (!base::CreateDirectory(
            cache_root_directory.Append(kTemporaryFileDirectory))) {
      LOG(WARNING) << "Failed to create directories.";
      return FILE_ERROR_FAILED;
    }
  }
  // Files in temporary directory need not persist across sessions. Clean up
  // the directory content while initialization. The directory itself should not
  // be deleted because it's created by cryptohome in clear and shouldn't be
  // re-created as encrypted.
  DeleteDirectoryContents(cache_root_directory.Append(kTemporaryFileDirectory));
  if (!base::CreateDirectory(cache_root_directory.Append(
          kMetadataDirectory)) ||
      !base::CreateDirectory(cache_root_directory.Append(
          kCacheFileDirectory))) {
    LOG(WARNING) << "Failed to create directories.";
    return FILE_ERROR_FAILED;
  }

  // Change permissions of cache file directory to u+rwx,og+x (711) in order to
  // allow archive files in that directory to be mounted by cros-disks.
  base::SetPosixFilePermissions(
      cache_root_directory.Append(kCacheFileDirectory),
      base::FILE_PERMISSION_USER_MASK |
      base::FILE_PERMISSION_EXECUTE_BY_GROUP |
      base::FILE_PERMISSION_EXECUTE_BY_OTHERS);

  // If attempting to migrate to DriveFS without previous Drive sync data
  // present, skip the migration.
  if (base::IsDirectoryEmpty(cache_root_directory.Append(kMetadataDirectory))) {
    return FILE_ERROR_FAILED;
  }

  internal::ResourceMetadataStorage::UpgradeOldDB(
      metadata_storage->directory_path());

  if (!metadata_storage->Initialize()) {
    LOG(WARNING) << "Failed to initialize the metadata storage.";
    return FILE_ERROR_FAILED;
  }

  return FILE_ERROR_OK;
}

base::FilePath GetFullPath(internal::ResourceMetadataStorage* metadata_storage,
                           const ResourceEntry& entry) {
  std::vector<std::string> path_components;
  ResourceEntry current_entry = entry;
  constexpr int kPathComponentSanityLimit = 100;
  for (int i = 0; i < kPathComponentSanityLimit; ++i) {
    path_components.push_back(current_entry.base_name());
    if (!current_entry.has_parent_local_id()) {
      // Ignore anything not contained within the drive grand root.
      return {};
    }
    if (current_entry.parent_local_id() == util::kDriveGrandRootLocalId) {
      // Omit the DriveGrantRoot directory from the path; DriveFS paths are
      // relative to the mount point.
      break;
    }
    if (metadata_storage->GetEntry(current_entry.parent_local_id(),
                                   &current_entry) != FILE_ERROR_OK) {
      return {};
    }
  }
  if (path_components.empty()) {
    return {};
  }
  base::FilePath path("/");
  for (auto it = path_components.crbegin(); it != path_components.crend();
       ++it) {
    path = path.Append(*it);
  }
  return path;
}

// Recover any dirty files in GCache/v1 to a recovered files directory in
// Downloads. This imitates the behavior of recovering cache files when database
// corruption occurs; however, in this case, we have an intact database so can
// use the exact file names, potentially with uniquifiers added since the
// directory structure is discarded.
void RecoverDirtyFiles(
    const base::FilePath& cache_directory,
    const base::FilePath& downloads_directory,
    const std::vector<std::pair<base::FilePath, std::string>>& dirty_files) {
  if (dirty_files.empty()) {
    return;
  }
  auto recovery_directory = GetRecoveredFilesPath(downloads_directory);
  if (!base::CreateDirectory(recovery_directory)) {
    return;
  }
  for (auto& dirty_file : dirty_files) {
    auto target_path =
        FindUniquePath(recovery_directory.Append(dirty_file.first.BaseName()));
    base::Move(cache_directory.Append(dirty_file.second), target_path);
  }
}

// Remove the data used by the old Drive client, first moving any dirty files
// into the user's Downloads.
void CleanupGCacheV1(
    const base::FilePath& cache_directory,
    const base::FilePath& downloads_directory,
    std::vector<std::pair<base::FilePath, std::string>> dirty_files) {
  RecoverDirtyFiles(cache_directory.Append(kCacheFileDirectory),
                    downloads_directory, dirty_files);
  DeleteDirectoryContents(cache_directory);
}

std::vector<base::FilePath> GetPinnedAndDirtyFiles(
    std::unique_ptr<internal::ResourceMetadataStorage, util::DestroyHelper>
        metadata_storage,
    base::FilePath cache_directory,
    base::FilePath downloads_directory) {
  std::vector<base::FilePath> pinned_files;
  std::vector<std::pair<base::FilePath, std::string>> dirty_files;
  for (auto it = metadata_storage->GetIterator(); !it->IsAtEnd();
       it->Advance()) {
    const auto& value = it->GetValue();
    if (!value.has_file_specific_info()) {
      continue;
    }
    const auto& info = value.file_specific_info();
    if (info.cache_state().is_pinned()) {
      auto path = GetFullPath(metadata_storage.get(), value);
      if (!path.empty()) {
        pinned_files.push_back(std::move(path));
      }
    }
    if (info.cache_state().is_dirty()) {
      dirty_files.push_back(std::make_pair(
          GetFullPath(metadata_storage.get(), value), value.local_id()));
    }
  }
  // Destructing |metadata_storage| requires a posted task to run, so defer
  // deleting its data until after it's been destructed. This also returns the
  // list of files to pin to the UI thread without waiting for the remaining
  // data to be cleared.
  metadata_storage.reset();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CleanupGCacheV1, std::move(cache_directory),
                     std::move(downloads_directory), std::move(dirty_files)));
  return pinned_files;
}

DriveMountStatus ConvertMountFailure(
    drivefs::DriveFsHost::MountObserver::MountFailure failure) {
  switch (failure) {
    case drivefs::DriveFsHost::MountObserver::MountFailure::kInvocation:
      return DriveMountStatus::kInvocationFailure;
    case drivefs::DriveFsHost::MountObserver::MountFailure::kIpcDisconnect:
      return DriveMountStatus::kUnexpectedDisconnect;
    case drivefs::DriveFsHost::MountObserver::MountFailure::kNeedsRestart:
      return DriveMountStatus::kTemporaryUnavailable;
    case drivefs::DriveFsHost::MountObserver::MountFailure::kTimeout:
      return DriveMountStatus::kTimeout;
    case drivefs::DriveFsHost::MountObserver::MountFailure::kUnknown:
      return DriveMountStatus::kUnknownFailure;
  }
  NOTREACHED();
}

bool EnsureDirectoryExists(const base::FilePath& path) {
  return base::DirectoryExists(path) || base::CreateDirectory(path);
}

void UmaEmitMountStatus(DriveMountStatus status) {
  UMA_HISTOGRAM_ENUMERATION("DriveCommon.Lifecycle.Mount", status);
}

void UmaEmitMountTime(DriveMountStatus status,
                      const base::TimeTicks& time_started) {
  if (status == DriveMountStatus::kSuccess) {
    UMA_HISTOGRAM_MEDIUM_TIMES("DriveCommon.Lifecycle.MountTime.SuccessTime",
                               base::TimeTicks::Now() - time_started);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES("DriveCommon.Lifecycle.MountTime.FailTime",
                               base::TimeTicks::Now() - time_started);
  }
}

void UmaEmitMountOutcome(DriveMountStatus status,
                         const base::TimeTicks& time_started) {
  UmaEmitMountStatus(status);
  UmaEmitMountTime(status, time_started);
}

void UmaEmitUnmountOutcome(DriveMountStatus status) {
  UMA_HISTOGRAM_ENUMERATION("DriveCommon.Lifecycle.Unmount", status);
}

void UmaEmitFirstLaunch(const base::TimeTicks& time_started) {
  UMA_HISTOGRAM_MEDIUM_TIMES("DriveCommon.Lifecycle.FirstLaunchTime",
                             base::TimeTicks::Now() - time_started);
}

}  // namespace

// Observes drive disable Preference's change.
class DriveIntegrationService::PreferenceWatcher
    : public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public chromeos::NetworkPortalDetector::Observer {
 public:
  explicit PreferenceWatcher(PrefService* pref_service)
      : pref_service_(pref_service), integration_service_(nullptr) {
    DCHECK(pref_service);
    pref_change_registrar_.Init(pref_service);
    pref_change_registrar_.Add(
        prefs::kDisableDrive,
        base::BindRepeating(&PreferenceWatcher::OnPreferenceChanged,
                            weak_ptr_factory_.GetWeakPtr()));
    pref_change_registrar_.Add(
        prefs::kDisableDriveOverCellular,
        base::BindRepeating(&PreferenceWatcher::UpdateSyncPauseState,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  ~PreferenceWatcher() override {
    if (integration_service_) {
      content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
          this);
      chromeos::network_portal_detector::GetInstance()->RemoveObserver(this);
    }
  }

  void set_integration_service(DriveIntegrationService* integration_service) {
    integration_service_ = integration_service;
    content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);

    // The NetworkPortalDetector instance may not be ready yet, so defer
    // accessing it.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DriveIntegrationService::PreferenceWatcher::
                                      AddNetworkPortalDetectorObserver,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  void UpdateSyncPauseState() {
    auto type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
    if (content::GetNetworkConnectionTracker()->GetConnectionType(
            &type, base::BindOnce(&DriveIntegrationService::PreferenceWatcher::
                                      OnConnectionChanged,
                                  weak_ptr_factory_.GetWeakPtr()))) {
      OnConnectionChanged(type);
    }
  }

  bool is_offline() const {
    return last_portal_status_ !=
               chromeos::NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE &&
           last_portal_status_ !=
               chromeos::NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN;
  }

 private:
  void OnPreferenceChanged() {
    DCHECK(integration_service_);
    integration_service_->SetEnabled(
        !pref_service_->GetBoolean(prefs::kDisableDrive));
  }

  void AddNetworkPortalDetectorObserver() {
    chromeos::network_portal_detector::GetInstance()->AddAndFireObserver(this);
  }

  // chromeos::NetworkPortalDetector::Observer
  void OnPortalDetectionCompleted(
      const chromeos::NetworkState* network,
      const chromeos::NetworkPortalDetector::CaptivePortalState& state)
      override {
    last_portal_status_ = state.status;

    if (integration_service_->remount_when_online_ &&
        last_portal_status_ ==
            chromeos::NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) {
      integration_service_->remount_when_online_ = false;
      integration_service_->mount_start_ = {};
      integration_service_->AddDriveMountPoint();
    }
  }

  // network::NetworkConnectionTracker::NetworkConnectionObserver
  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    if (!integration_service_->GetDriveFsInterface())
      return;

    integration_service_->GetDriveFsInterface()->UpdateNetworkState(
        network::NetworkConnectionTracker::IsConnectionCellular(type) &&
            pref_service_->GetBoolean(prefs::kDisableDriveOverCellular),
        type == network::mojom::ConnectionType::CONNECTION_NONE);
  }

  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  DriveIntegrationService* integration_service_;
  chromeos::NetworkPortalDetector::CaptivePortalStatus last_portal_status_ =
      chromeos::NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN;

  base::WeakPtrFactory<PreferenceWatcher> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(PreferenceWatcher);
};

class DriveIntegrationService::DriveFsHolder
    : public drivefs::DriveFsHost::Delegate,
      public drivefs::DriveFsHost::MountObserver {
 public:
  DriveFsHolder(Profile* profile,
                drivefs::DriveFsHost::MountObserver* mount_observer,
                DriveFsMojoListenerFactory test_drivefs_mojo_listener_factory)
      : profile_(profile),
        mount_observer_(mount_observer),
        test_drivefs_mojo_listener_factory_(
            std::move(test_drivefs_mojo_listener_factory)),
        drivefs_host_(profile_->GetPath(),
                      this,
                      this,
                      content::GetNetworkConnectionTracker(),
                      base::DefaultClock::GetInstance(),
                      chromeos::disks::DiskMountManager::GetInstance(),
                      std::make_unique<base::OneShotTimer>()) {}

  drivefs::DriveFsHost* drivefs_host() { return &drivefs_host_; }

 private:
  // drivefs::DriveFsHost::Delegate:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return profile_->GetURLLoaderFactory();
  }

  signin::IdentityManager* GetIdentityManager() override {
    return IdentityManagerFactory::GetForProfile(profile_);
  }

  const AccountId& GetAccountId() override {
    return chromeos::ProfileHelper::Get()
        ->GetUserByProfile(profile_)
        ->GetAccountId();
  }

  std::string GetObfuscatedAccountId() override {
    if (!GetAccountId().HasAccountIdKey()) {
      return "";
    }
    return base::MD5String(GetProfileSalt() + "-" +
                           GetAccountId().GetAccountIdKey());
  }

  bool IsMetricsCollectionEnabled() override {
    return g_browser_process->local_state()->GetBoolean(
        metrics::prefs::kMetricsReportingEnabled);
  }

  DriveNotificationManager& GetDriveNotificationManager() override {
    return *DriveNotificationManagerFactory::GetForBrowserContext(profile_);
  }

  void OnMountFailed(MountFailure failure,
                     base::Optional<base::TimeDelta> remount_delay) override {
    mount_observer_->OnMountFailed(failure, std::move(remount_delay));
  }

  void OnMounted(const base::FilePath& path) override {
    mount_observer_->OnMounted(path);
  }

  void OnUnmounted(base::Optional<base::TimeDelta> remount_delay) override {
    mount_observer_->OnUnmounted(std::move(remount_delay));
  }

  const std::string& GetProfileSalt() {
    if (!profile_salt_.empty()) {
      return profile_salt_;
    }
    PrefService* prefs = profile_->GetPrefs();
    profile_salt_ = prefs->GetString(prefs::kDriveFsProfileSalt);
    if (profile_salt_.empty()) {
      profile_salt_ = base::UnguessableToken::Create().ToString();
      prefs->SetString(prefs::kDriveFsProfileSalt, profile_salt_);
    }
    return profile_salt_;
  }

  std::unique_ptr<drivefs::DriveFsBootstrapListener> CreateMojoListener()
      override {
    if (test_drivefs_mojo_listener_factory_)
      return test_drivefs_mojo_listener_factory_.Run();
    return Delegate::CreateMojoListener();
  }

  base::FilePath GetMyFilesPath() override {
    return file_manager::util::GetMyFilesFolderForProfile(profile_);
  }

  std::string GetLostAndFoundDirectoryName() override {
    return l10n_util::GetStringUTF8(
        IDS_FILE_BROWSER_RECOVERED_FILES_FROM_GOOGLE_DRIVE_DIRECTORY_NAME);
  }

  bool IsVerboseLoggingEnabled() override {
    return profile_->GetPrefs()->GetBoolean(
        prefs::kDriveFsEnableVerboseLogging);
  }

  drivefs::mojom::DriveFsDelegate::ExtensionConnectionStatus ConnectToExtension(
      drivefs::mojom::ExtensionConnectionParamsPtr params,
      mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort> port,
      mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> host) override {
    return ConnectToDriveFsNativeMessageExtension(
        profile_, params->extension_id, std::move(port), std::move(host));
  }

  Profile* const profile_;
  drivefs::DriveFsHost::MountObserver* const mount_observer_;

  const DriveFsMojoListenerFactory test_drivefs_mojo_listener_factory_;

  drivefs::DriveFsHost drivefs_host_;

  std::string profile_salt_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsHolder);
};

DriveIntegrationService::DriveIntegrationService(
    Profile* profile,
    const std::string& test_mount_point_name,
    const base::FilePath& test_cache_root,
    DriveFsMojoListenerFactory test_drivefs_mojo_listener_factory)
    : profile_(profile),
      state_(NOT_INITIALIZED),
      enabled_(false),
      mount_point_name_(test_mount_point_name),
      cache_root_directory_(!test_cache_root.empty()
                                ? test_cache_root
                                : util::GetCacheRootPath(profile)),
      drivefs_holder_(std::make_unique<DriveFsHolder>(
          profile_,
          this,
          std::move(test_drivefs_mojo_listener_factory))),
      power_manager_observer_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile && !profile->IsOffTheRecord());

  logger_ = std::make_unique<EventLogger>();
  blocking_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::WithBaseSyncPrimitives()});

  if (util::IsDriveEnabledForProfile(profile)) {
    preference_watcher_ =
        std::make_unique<PreferenceWatcher>(profile->GetPrefs());
    preference_watcher_->set_integration_service(this);
  }

  bool migrated_to_drivefs =
      profile_->GetPrefs()->GetBoolean(prefs::kDriveFsPinnedMigrated);
  if (migrated_to_drivefs) {
    state_ = INITIALIZED;
  } else {
    metadata_storage_.reset(new internal::ResourceMetadataStorage(
        cache_root_directory_.Append(kMetadataDirectory),
        blocking_task_runner_.get()));
  }

  // PowerManagerClient is unset in unit tests.
  if (chromeos::PowerManagerClient::Get()) {
    power_manager_observer_.Add(chromeos::PowerManagerClient::Get());
  }
  SetEnabled(drive::util::IsDriveEnabledForProfile(profile));
}

DriveIntegrationService::~DriveIntegrationService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void DriveIntegrationService::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  weak_ptr_factory_.InvalidateWeakPtrs();

  RemoveDriveMountPoint();
}

void DriveIntegrationService::SetEnabled(bool enabled) {
  // If Drive is being disabled, ensure the download destination preference to
  // be out of Drive. Do this before "Do nothing if not changed." because we
  // want to run the check for the first SetEnabled() called in the constructor,
  // which may be a change from false to false.
  if (!enabled)
    AvoidDriveAsDownloadDirectoryPreference();

  // Do nothing if not changed.
  if (enabled_ == enabled)
    return;

  if (enabled) {
    enabled_ = true;
    switch (state_) {
      case NOT_INITIALIZED:
        // If the initialization is not yet done, trigger it.
        Initialize();
        return;

      case INITIALIZING:
      case REMOUNTING:
        // If the state is INITIALIZING or REMOUNTING, at the end of the
        // process, it tries to mounting (with re-checking enabled state).
        // Do nothing for now.
        return;

      case INITIALIZED:
        // The integration service is already initialized. Add the mount point.
        AddDriveMountPoint();
        return;
    }
    NOTREACHED();
  } else {
    RemoveDriveMountPoint();
    enabled_ = false;
    drivefs_total_failures_count_ = 0;
    drivefs_consecutive_failures_count_ = 0;
    mount_failed_ = false;
    mount_start_ = {};
  }
}

bool DriveIntegrationService::IsMounted() const {
  if (mount_point_name_.empty())
    return false;

  // Look up the registered path, and just discard it.
  // GetRegisteredPath() returns true if the path is available.
  base::FilePath unused;
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);
  return mount_points->GetRegisteredPath(mount_point_name_, &unused);
}

base::FilePath DriveIntegrationService::GetMountPointPath() const {
  return drivefs_holder_->drivefs_host()->GetMountPath();
}

base::FilePath DriveIntegrationService::GetDriveFsLogPath() const {
  return drivefs_holder_->drivefs_host()->GetDataPath().Append(
      "Logs/drivefs.txt");
}

bool DriveIntegrationService::GetRelativeDrivePath(
    const base::FilePath& local_path,
    base::FilePath* drive_path) const {
  if (!IsMounted()) {
    return false;
  }
  base::FilePath mount_point = GetMountPointPath();
  base::FilePath relative("/");
  if (!mount_point.AppendRelativePath(local_path, &relative)) {
    return false;
  }
  if (drive_path) {
    *drive_path = relative;
  }
  return true;
}

void DriveIntegrationService::AddObserver(
    DriveIntegrationServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void DriveIntegrationService::RemoveObserver(
    DriveIntegrationServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

void DriveIntegrationService::ClearCacheAndRemountFileSystem(
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (in_clear_cache_) {
    std::move(callback).Run(false);
    return;
  }
  in_clear_cache_ = true;

  if (IsMounted()) {
    RemoveDriveMountPoint();
    // TODO(crbug/1069328): We wait 2 seconds here so that DriveFS can unmount
    // completely. Ideally we'd wait for an unmount complete callback.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DriveIntegrationService::
                           ClearCacheAndRemountFileSystemAfterUnmount,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::TimeDelta::FromSeconds(2));
  } else {
    ClearCacheAndRemountFileSystemAfterUnmount(std::move(callback));
  }
}

void DriveIntegrationService::ClearCacheAndRemountFileSystemAfterUnmount(
    base::OnceCallback<void(bool)> callback) {
  bool success = true;
  base::FilePath cache_path = GetDriveFsHost()->GetDataPath();
  base::FilePath logs_path = GetDriveFsLogPath().DirName();
  base::FileEnumerator content_enumerator(
      cache_path, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
          base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath path = content_enumerator.Next(); !path.empty();
       path = content_enumerator.Next()) {
    // Keep the logs folder as it's useful for debugging.
    if (path == logs_path) {
      continue;
    }
    if (!base::DeletePathRecursively(path)) {
      success = false;
      break;
    }
  }

  if (is_enabled()) {
    AddDriveMountPoint();
  }
  in_clear_cache_ = false;
  std::move(callback).Run(success);
}

drivefs::DriveFsHost* DriveIntegrationService::GetDriveFsHost() const {
  return drivefs_holder_->drivefs_host();
}

drivefs::mojom::DriveFs* DriveIntegrationService::GetDriveFsInterface() const {
  return drivefs_holder_->drivefs_host()->GetDriveFsInterface();
}

void DriveIntegrationService::AddBackDriveMountPoint(
    const base::Callback<void(bool)>& callback,
    FileError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(callback);

  state_ = error == FILE_ERROR_OK ? INITIALIZED : NOT_INITIALIZED;

  if (error != FILE_ERROR_OK || !enabled_) {
    // Failed to reset, or Drive was disabled during the reset.
    callback.Run(false);
    return;
  }

  AddDriveMountPoint();
  callback.Run(true);
}

void DriveIntegrationService::AddDriveMountPoint() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZED, state_);
  DCHECK(enabled_);

  weak_ptr_factory_.InvalidateWeakPtrs();

  if (!drivefs_holder_->drivefs_host()->IsMounted()) {
    PrefService* prefs = profile_->GetPrefs();
    bool was_ever_mounted =
        prefs->GetBoolean(prefs::kDriveFsWasLaunchedAtLeastOnce);
    if (mount_start_.is_null() || was_ever_mounted) {
      mount_start_ = base::TimeTicks::Now();
    }
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_.get(), FROM_HERE,
        base::BindOnce(&EnsureDirectoryExists,
                       drivefs_holder_->drivefs_host()->GetDataPath()),
        base::BindOnce(&DriveIntegrationService::MaybeMountDrive,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    AddDriveMountPointAfterMounted();
  }
}

void DriveIntegrationService::MaybeMountDrive(bool data_directory_exists) {
  if (!data_directory_exists) {
    LOG(ERROR) << "Could not create DriveFS data directory";
  } else {
    drivefs_holder_->drivefs_host()->Mount();
  }
}

bool DriveIntegrationService::AddDriveMountPointAfterMounted() {
  const base::FilePath& drive_mount_point = GetMountPointPath();
  if (mount_point_name_.empty()) {
    mount_point_name_ = drive_mount_point.BaseName().AsUTF8Unsafe();
  }
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);
  drivefs_consecutive_failures_count_ = 0;

  bool success = mount_points->RegisterFileSystem(
      mount_point_name_, storage::kFileSystemTypeDriveFs,
      storage::FileSystemMountOption(), drive_mount_point);

  if (success) {
    logger_->Log(logging::LOG_INFO, "Drive mount point is added");
    for (auto& observer : observers_)
      observer.OnFileSystemMounted();
  }

  if (preference_watcher_) {
    preference_watcher_->UpdateSyncPauseState();
  }
  if (!profile_->GetPrefs()->GetBoolean(prefs::kDriveFsPinnedMigrated)) {
    MigratePinnedFiles();
  }

  return success;
}

void DriveIntegrationService::RemoveDriveMountPoint() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  weak_ptr_factory_.InvalidateWeakPtrs();
  remount_when_online_ = false;

  if (!mount_point_name_.empty()) {
    if (storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
            mount_point_name_)) {
      for (auto& observer : observers_) {
        observer.OnFileSystemBeingUnmounted();
      }
      logger_->Log(logging::LOG_INFO, "Drive mount point is removed");
    }
  }
  drivefs_holder_->drivefs_host()->Unmount();
}

void DriveIntegrationService::MaybeRemountFileSystem(
    base::Optional<base::TimeDelta> remount_delay,
    bool failed_to_mount) {
  DCHECK_EQ(INITIALIZED, state_);

  RemoveDriveMountPoint();

  if (!remount_delay) {
    if (failed_to_mount && preference_watcher_ &&
        preference_watcher_->is_offline()) {
      logger_->Log(logging::LOG_WARNING,
                   "DriveFs failed to start, will retry when online.");
      remount_when_online_ = true;
      return;
    }
    // If DriveFs didn't specify retry time it's likely unexpected error, e.g.
    // crash. Use limited exponential backoff for retry.
    ++drivefs_consecutive_failures_count_;
    ++drivefs_total_failures_count_;
    if (drivefs_total_failures_count_ > 10) {
      mount_failed_ = true;
      logger_->Log(logging::LOG_ERROR,
                   "DriveFs is too crashy. Leaving it alone.");
      for (auto& observer : observers_)
        observer.OnFileSystemMountFailed();
      return;
    }
    if (drivefs_consecutive_failures_count_ > 3) {
      mount_failed_ = true;
      logger_->Log(logging::LOG_ERROR,
                   "DriveFs keeps failing at start. Giving up.");
      for (auto& observer : observers_)
        observer.OnFileSystemMountFailed();
      return;
    }
    remount_delay = base::TimeDelta::FromSeconds(
        5 * (1 << (drivefs_consecutive_failures_count_ - 1)));
    logger_->Log(logging::LOG_WARNING, "DriveFs died, retry in %d seconds",
                 static_cast<int>(remount_delay.value().InSeconds()));
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DriveIntegrationService::AddDriveMountPoint,
                     weak_ptr_factory_.GetWeakPtr()),
      remount_delay.value());
}

void DriveIntegrationService::OnMounted(const base::FilePath& mount_path) {
  if (AddDriveMountPointAfterMounted()) {
    PrefService* prefs = profile_->GetPrefs();
    bool was_ever_mounted =
        prefs->GetBoolean(prefs::kDriveFsWasLaunchedAtLeastOnce);
    if (was_ever_mounted) {
      UmaEmitMountOutcome(DriveMountStatus::kSuccess, mount_start_);
    } else {
      UmaEmitFirstLaunch(mount_start_);
      prefs->SetBoolean(prefs::kDriveFsWasLaunchedAtLeastOnce, true);
    }
  } else {
    UmaEmitMountOutcome(DriveMountStatus::kUnknownFailure, mount_start_);
  }
}

void DriveIntegrationService::OnUnmounted(
    base::Optional<base::TimeDelta> remount_delay) {
  UmaEmitUnmountOutcome(remount_delay ? DriveMountStatus::kTemporaryUnavailable
                                      : DriveMountStatus::kUnknownFailure);
  MaybeRemountFileSystem(remount_delay, false);
}

void DriveIntegrationService::OnMountFailed(
    MountFailure failure,
    base::Optional<base::TimeDelta> remount_delay) {
  PrefService* prefs = profile_->GetPrefs();
  DriveMountStatus status = ConvertMountFailure(failure);
  UmaEmitMountStatus(status);
  bool was_ever_mounted =
      prefs->GetBoolean(prefs::kDriveFsWasLaunchedAtLeastOnce);
  if (was_ever_mounted) {
    UmaEmitMountTime(status, mount_start_);
  } else {
    // We don't record mount time until we mount successfully at least once.
  }
  MaybeRemountFileSystem(remount_delay, true);
}

void DriveIntegrationService::Initialize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(NOT_INITIALIZED, state_);
  DCHECK(enabled_);

  state_ = INITIALIZING;

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &InitializeMetadata, cache_root_directory_, metadata_storage_.get(),
          file_manager::util::GetDownloadsFolderForProfile(profile_)),
      base::BindOnce(
          &DriveIntegrationService::InitializeAfterMetadataInitialized,
          weak_ptr_factory_.GetWeakPtr()));
}

void DriveIntegrationService::InitializeAfterMetadataInitialized(
    FileError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZING, state_);

  if (error != FILE_ERROR_OK) {
    profile_->GetPrefs()->SetBoolean(prefs::kDriveFsPinnedMigrated, true);
    metadata_storage_.reset();
    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CleanupGCacheV1, cache_root_directory_,
                       base::FilePath(),
                       std::vector<std::pair<base::FilePath, std::string>>()));
  }
  state_ = INITIALIZED;
  if (enabled_)
    AddDriveMountPoint();
}

void DriveIntegrationService::AvoidDriveAsDownloadDirectoryPreference() {
  if (DownloadDirectoryPreferenceIsInDrive()) {
    profile_->GetPrefs()->SetFilePath(
        ::prefs::kDownloadDefaultDirectory,
        file_manager::util::GetDownloadsFolderForProfile(profile_));
  }
}

bool DriveIntegrationService::DownloadDirectoryPreferenceIsInDrive() {
  const auto downloads_path =
      profile_->GetPrefs()->GetFilePath(::prefs::kDownloadDefaultDirectory);
  const auto* user = chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  return user && user->GetAccountId().HasAccountIdKey() &&
         GetMountPointPath().IsParent(downloads_path);
}

void DriveIntegrationService::MigratePinnedFiles() {
  if (!metadata_storage_)
    return;

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &GetPinnedAndDirtyFiles, std::move(metadata_storage_),
          cache_root_directory_,
          file_manager::util::GetDownloadsFolderForProfile(profile_)),
      base::BindOnce(&DriveIntegrationService::PinFiles,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveIntegrationService::PinFiles(
    const std::vector<base::FilePath>& files_to_pin) {
  if (!drivefs_holder_->drivefs_host()->IsMounted())
    return;

  for (const auto& path : files_to_pin) {
    GetDriveFsInterface()->SetPinned(path, true, base::DoNothing());
  }
  profile_->GetPrefs()->SetBoolean(prefs::kDriveFsPinnedMigrated, true);
}

void DriveIntegrationService::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  // This may a bit racy since it doesn't prevent suspend until the unmount is
  // completed, instead relying on something else to defer suspending long
  // enough.
  RemoveDriveMountPoint();
}

void DriveIntegrationService::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  if (is_enabled()) {
    AddDriveMountPoint();
  }
}

void DriveIntegrationService::GetQuickAccessItems(
    int max_number,
    GetQuickAccessItemsCallback callback) {
  if (!GetDriveFsHost()) {
    std::move(callback).Run(drive::FileError::FILE_ERROR_SERVICE_UNAVAILABLE,
                            {});
    return;
  }

  auto query = drivefs::mojom::QueryParameters::New();
  query->page_size = max_number;
  query->query_kind = drivefs::mojom::QueryKind::kQuickAccess;

  auto on_response =
      base::BindOnce(&DriveIntegrationService::OnGetQuickAccessItems,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  GetDriveFsHost()->PerformSearch(
      std::move(query),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(on_response), drive::FileError::FILE_ERROR_ABORT,
          base::Optional<std::vector<drivefs::mojom::QueryItemPtr>>()));
}

void DriveIntegrationService::OnGetQuickAccessItems(
    GetQuickAccessItemsCallback callback,
    drive::FileError error,
    base::Optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  if (error != drive::FILE_ERROR_OK || !items.has_value()) {
    std::move(callback).Run(error, {});
    return;
  }

  std::vector<QuickAccessItem> result;
  result.reserve(items->size());
  for (const auto& item : *items) {
    result.push_back({item->path, item->metadata->quick_access->score});
  }
  std::move(callback).Run(error, std::move(result));
}

void DriveIntegrationService::GetMetadata(
    const base::FilePath& local_path,
    drivefs::mojom::DriveFs::GetMetadataCallback callback) {
  if (!IsMounted() || !GetDriveFsInterface()) {
    std::move(callback).Run(drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr);
    return;
  }

  base::FilePath drive_path;
  if (!GetRelativeDrivePath(local_path, &drive_path)) {
    std::move(callback).Run(drive::FILE_ERROR_NOT_FOUND, nullptr);
    return;
  }

  GetDriveFsInterface()->GetMetadata(
      drive_path,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr));
}

void DriveIntegrationService::LocateFilesByItemIds(
    const std::vector<std::string>& item_ids,
    drivefs::mojom::DriveFs::LocateFilesByItemIdsCallback callback) {
  if (!IsMounted() || !GetDriveFsInterface()) {
    std::move(callback).Run({});
  }
  GetDriveFsInterface()->LocateFilesByItemIds(item_ids, std::move(callback));
}

void DriveIntegrationService::RestartDrive() {
  MaybeRemountFileSystem(base::TimeDelta(), false);
}

void DriveIntegrationService::SetStartupArguments(
    std::string arguments,
    base::OnceCallback<void(bool)> callback) {
  if (!GetDriveFsInterface()) {
    std::move(callback).Run(false);
    return;
  }
  GetDriveFsInterface()->SetStartupArguments(arguments, std::move(callback));
}

void DriveIntegrationService::GetStartupArguments(
    base::OnceCallback<void(const std::string&)> callback) {
  if (!GetDriveFsInterface()) {
    std::move(callback).Run("");
    return;
  }
  GetDriveFsInterface()->GetStartupArguments(std::move(callback));
}

void DriveIntegrationService::SetTracingEnabled(bool enabled) {
  if (GetDriveFsInterface()) {
    GetDriveFsInterface()->SetTracingEnabled(enabled);
  }
}

void DriveIntegrationService::SetNetworkingEnabled(bool enabled) {
  if (GetDriveFsInterface()) {
    GetDriveFsInterface()->SetNetworkingEnabled(enabled);
  }
}

void DriveIntegrationService::ForcePauseSyncing(bool enabled) {
  if (GetDriveFsInterface()) {
    GetDriveFsInterface()->ForcePauseSyncing(enabled);
  }
}

void DriveIntegrationService::DumpAccountSettings() {
  if (GetDriveFsInterface()) {
    GetDriveFsInterface()->DumpAccountSettings();
  }
}

void DriveIntegrationService::LoadAccountSettings() {
  if (GetDriveFsInterface()) {
    GetDriveFsInterface()->LoadAccountSettings();
  }
}

//===================== DriveIntegrationServiceFactory =======================

DriveIntegrationServiceFactory::FactoryCallback*
    DriveIntegrationServiceFactory::factory_for_test_ = nullptr;

DriveIntegrationServiceFactory::ScopedFactoryForTest::ScopedFactoryForTest(
    FactoryCallback* factory_for_test) {
  factory_for_test_ = factory_for_test;
}

DriveIntegrationServiceFactory::ScopedFactoryForTest::~ScopedFactoryForTest() {
  factory_for_test_ = nullptr;
}

// static
DriveIntegrationService* DriveIntegrationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DriveIntegrationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DriveIntegrationService* DriveIntegrationServiceFactory::FindForProfile(
    Profile* profile) {
  return static_cast<DriveIntegrationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
DriveIntegrationServiceFactory* DriveIntegrationServiceFactory::GetInstance() {
  return base::Singleton<DriveIntegrationServiceFactory>::get();
}

DriveIntegrationServiceFactory::DriveIntegrationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DriveIntegrationService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(DriveNotificationManagerFactory::GetInstance());
  DependsOn(DownloadCoreServiceFactory::GetInstance());
}

DriveIntegrationServiceFactory::~DriveIntegrationServiceFactory() = default;

content::BrowserContext* DriveIntegrationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* DriveIntegrationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  DriveIntegrationService* service = nullptr;
  if (!factory_for_test_) {
    service =
        new DriveIntegrationService(profile, std::string(), base::FilePath());
  } else {
    service = factory_for_test_->Run(profile);
  }

  return service;
}

}  // namespace drive
