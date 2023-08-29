// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/drive_integration_service.h"

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/adapters.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/system_notification_manager.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/drivefs/drivefs_bootstrap.h"
#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"
#include "chromeos/ash/components/drivefs/sync_status_tracker.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/components/drivefs/mojom/drivefs_native_messaging.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/drive_notification_manager.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_errors.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/resource_metadata_storage.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/user_agent.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace drive {
namespace {

using base::Seconds;
using base::SequencedTaskRunner;
using base::TimeDelta;
using content::BrowserContext;
using content::BrowserThread;
using drivefs::mojom::DriveFs;
using drivefs::pinning::PinManager;
using network::NetworkConnectionTracker;
using network::mojom::ConnectionType;
using prefs::kDriveFsBulkPinningEnabled;

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
      dir, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
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
    if (base::SysInfo::IsRunningOnChromeOS()) {
      LOG(ERROR) << "/tmp should have been created as clear.";
    }
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
  if (!base::CreateDirectory(cache_root_directory.Append(kMetadataDirectory)) ||
      !base::CreateDirectory(
          cache_root_directory.Append(kCacheFileDirectory))) {
    LOG(WARNING) << "Failed to create directories.";
    return FILE_ERROR_FAILED;
  }

  // Change permissions of cache file directory to u+rwx,og+x (711) in order to
  // allow archive files in that directory to be mounted by cros-disks.
  base::SetPosixFilePermissions(
      cache_root_directory.Append(kCacheFileDirectory),
      base::FILE_PERMISSION_USER_MASK | base::FILE_PERMISSION_EXECUTE_BY_GROUP |
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
  for (const std::string& component : base::Reversed(path_components)) {
    path = path.Append(component);
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
  SequencedTaskRunner::GetCurrentDefault()->PostTask(
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

// Clears the cache folder at |cache_path|, but preserve |logs_path|.
// |logs_path| should be a descendent of |cache_path|.
bool ClearCache(base::FilePath cache_path, base::FilePath logs_path) {
  DCHECK(cache_path.IsParent(logs_path));
  bool success = true;
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
  return success;
}

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "GoogleDrive.BulkPinning.MountFailureReason" in
// src/tools/metrics/histograms/enums.xml.
enum class BulkPinningMountFailureReason {
  kSuccess = 0,
  kThreeConsecutiveFailures = 1,
  kMoreThanTenTotalFailures = 2,
  kMaxValue = kMoreThanTenTotalFailures,
};

void RecordBulkPinningMountFailureReason(const Profile* profile,
                                         BulkPinningMountFailureReason reason) {
  if (!drive::util::IsDriveFsBulkPinningEnabled(profile)) {
    return;
  }
  base::UmaHistogramEnumeration(
      "FileBrowser.GoogleDrive.BulkPinning.MultipleMountFailures", reason);
}

}  // namespace

// Observes drive disable Preference's change.
class DriveIntegrationService::PreferenceWatcher
    : public NetworkConnectionTracker::NetworkConnectionObserver,
      public ash::NetworkStateHandlerObserver {
 public:
  using NetworkHandler = ash::NetworkHandler;
  using NetworkState = ash::NetworkState;
  using PortalState = NetworkState::PortalState;

  explicit PreferenceWatcher(Profile* profile)
      : profile_(profile), pref_service_(profile->GetPrefs()) {
    DCHECK(pref_service_);
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        prefs::kDisableDrive,
        base::BindRepeating(&PreferenceWatcher::OnPreferenceChanged,
                            weak_ptr_factory_.GetWeakPtr()));
    pref_change_registrar_.Add(
        prefs::kDisableDriveOverCellular,
        base::BindRepeating(&PreferenceWatcher::UpdateSyncPauseState,
                            weak_ptr_factory_.GetWeakPtr()));
    if (ash::features::IsDriveFsMirroringEnabled()) {
      pref_change_registrar_.Add(
          prefs::kDriveFsEnableMirrorSync,
          base::BindRepeating(&PreferenceWatcher::ToggleLocalMirroring,
                              weak_ptr_factory_.GetWeakPtr()));
    }

    if (util::IsDriveFsBulkPinningEnabled(profile_)) {
      pref_change_registrar_.Add(
          kDriveFsBulkPinningEnabled,
          base::BindRepeating(&PreferenceWatcher::ToggleBulkPinning,
                              weak_ptr_factory_.GetWeakPtr()));
    }
  }

  PreferenceWatcher(const PreferenceWatcher&) = delete;
  PreferenceWatcher& operator=(const PreferenceWatcher&) = delete;

  ~PreferenceWatcher() override {
    if (integration_service_) {
      content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
          this);
      if (NetworkHandler::IsInitialized()) {
        NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
      }
    }
  }

  void SetIntegrationService(DriveIntegrationService* integration_service) {
    integration_service_ = integration_service;
    content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);

    AddNetworkPortalDetectorObserver();
  }

  void UpdateSyncPauseState() {
    if (ConnectionType type = ConnectionType::CONNECTION_UNKNOWN;
        content::GetNetworkConnectionTracker()->GetConnectionType(
            &type, base::BindOnce(&DriveIntegrationService::PreferenceWatcher::
                                      OnConnectionChanged,
                                  weak_ptr_factory_.GetWeakPtr()))) {
      OnConnectionChanged(type);
    }
  }

  bool IsOnline() const { return portal_state_ == PortalState::kOnline; }

 private:
  void OnPreferenceChanged() {
    DCHECK(integration_service_);
    integration_service_->SetEnabled(
        !pref_service_->GetBoolean(prefs::kDisableDrive));
  }

  void ToggleLocalMirroring() {
    DCHECK(integration_service_);
    if (!ash::features::IsDriveFsMirroringEnabled()) {
      return;
    }

    if (pref_service_->GetBoolean(prefs::kDriveFsEnableMirrorSync)) {
      integration_service_->ToggleMirroring(
          true, base::BindOnce(
                    &DriveIntegrationService::OnEnableMirroringStatusUpdate,
                    integration_service_->weak_ptr_factory_.GetWeakPtr()));
    } else {
      integration_service_->ToggleMirroring(
          false, base::BindOnce(
                     &DriveIntegrationService::OnDisableMirroringStatusUpdate,
                     integration_service_->weak_ptr_factory_.GetWeakPtr()));
    }
  }

  void ToggleBulkPinning() {
    DCHECK(integration_service_);
    integration_service_->ToggleBulkPinning();
  }

  void AddNetworkPortalDetectorObserver() {
    if (!NetworkHandler::IsInitialized()) {
      return;  // Test environment.
    }
    ash::NetworkStateHandler* const handler =
        NetworkHandler::Get()->network_state_handler();
    DCHECK(handler);
    handler->AddObserver(this);
    const NetworkState* const network = handler->DefaultNetwork();
    PortalStateChanged(
        network, network ? network->GetPortalState() : PortalState::kUnknown);
  }

  // NetworkStateHandlerObserver
  void PortalStateChanged(const NetworkState* default_network,
                          PortalState portal_state) override {
    VLOG(1) << "PortalStateChanged: " << portal_state;
    portal_state_ = portal_state;

    if (integration_service_->remount_when_online_ && IsOnline()) {
      integration_service_->remount_when_online_ = false;
      integration_service_->mount_start_ = {};
      integration_service_->AddDriveMountPoint();
    }

    if (PinManager* const pin_manager = integration_service_->GetPinManager()) {
      pin_manager->SetOnline(IsOnline());
    }
  }

  void OnShuttingDown() override {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
  }

  // NetworkConnectionTracker::NetworkConnectionObserver
  void OnConnectionChanged(const ConnectionType type) override {
    DCHECK(integration_service_);

    const bool online = type != ConnectionType::CONNECTION_NONE;
    const bool pause_syncing =
        NetworkConnectionTracker::IsConnectionCellular(type) &&
        pref_service_->GetBoolean(prefs::kDisableDriveOverCellular);

    VLOG(1) << "OnConnectionChanged: {type: " << type << ", online: " << online
            << ", pause_syncing: " << pause_syncing << "}";

    integration_service_->UpdateNetworkState(pause_syncing, !online);
  }

  const raw_ptr<const Profile, ExperimentalAsh> profile_;
  const raw_ptr<PrefService, ExperimentalAsh> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<DriveIntegrationService, ExperimentalAsh> integration_service_ =
      nullptr;
  PortalState portal_state_ = PortalState::kUnknown;

  base::WeakPtrFactory<PreferenceWatcher> weak_ptr_factory_{this};
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
                      ash::disks::DiskMountManager::GetInstance(),
                      std::make_unique<base::OneShotTimer>()) {}

  DriveFsHolder(const DriveFsHolder&) = delete;
  DriveFsHolder& operator=(const DriveFsHolder&) = delete;

  drivefs::DriveFsHost* drivefs_host() { return &drivefs_host_; }

  void RegisterDriveFsNativeMessageHostBridge(
      mojo::PendingRemote<crosapi::mojom::DriveFsNativeMessageHostBridge>
          bridge) {
    if (native_message_host_bridge_) {
      // We only accept one registered bridge at a time as it doesn't make sense
      // for DriveFS to talk to multiple extensions at the same time.
      return;
    }
    native_message_host_bridge_.Bind(std::move(bridge));
    native_message_host_bridge_.reset_on_disconnect();

    if (pending_connect_to_extension_request_) {
      std::move(pending_connect_to_extension_request_).Run();
    }
    native_message_keep_alive_.reset();
  }

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
    return ash::ProfileHelper::Get()
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
                     absl::optional<TimeDelta> remount_delay) override {
    mount_observer_->OnMountFailed(failure, std::move(remount_delay));
  }

  void OnMounted(const base::FilePath& path) override {
    mount_observer_->OnMounted(path);
  }

  void OnUnmounted(absl::optional<TimeDelta> remount_delay) override {
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
    if (test_drivefs_mojo_listener_factory_) {
      return test_drivefs_mojo_listener_factory_.Run();
    }
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

  void ConnectToExtension(
      drivefs::mojom::ExtensionConnectionParamsPtr params,
      mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort> port,
      mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> host,
      drivefs::mojom::DriveFsDelegate::ConnectToExtensionCallback callback)
      override {
    if (crosapi::browser_util::IsLacrosPrimaryBrowser()) {
      if (!native_message_host_bridge_) {
        auto* browser_manager = crosapi::BrowserManager::Get();
        if (!native_message_keep_alive_ && browser_manager) {
          native_message_keep_alive_ = browser_manager->KeepAlive(
              crosapi::BrowserManager::Feature::kDriveFsNativeMessaging);
        }

        // DriveFS only sends one ConnectToExtension request at a time, so if
        // there is already an existing request, it means that DriveFS has
        // restarted and we can just drop the previous request.
        //
        // Unretained is fine here because this callback is owned and only
        // called by `this`.
        pending_connect_to_extension_request_ = base::BindOnce(
            &DriveFsHolder::ConnectToExtension, base::Unretained(this),
            std::move(params), std::move(port), std::move(host),
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                std::move(callback),
                drivefs::mojom::ExtensionConnectionStatus::kUnknownError));
        return;
      }
      native_message_host_bridge_->ConnectToExtension(
          std::move(params), std::move(port), std::move(host),
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback),
              drivefs::mojom::ExtensionConnectionStatus::kUnknownError));
    } else {
      std::move(callback).Run(ConnectToDriveFsNativeMessageExtension(
          profile_, params->extension_id, std::move(port), std::move(host)));
    }
  }

  const std::string GetMachineRootID() override {
    if (!ash::features::IsDriveFsMirroringEnabled()) {
      return "";
    }
    return profile_->GetPrefs()->GetString(
        prefs::kDriveFsMirrorSyncMachineRootId);
  }

  void PersistMachineRootID(const std::string& id) override {
    if (!ash::features::IsDriveFsMirroringEnabled()) {
      return;
    }
    profile_->GetPrefs()->SetString(prefs::kDriveFsMirrorSyncMachineRootId, id);
  }

  const raw_ptr<Profile, ExperimentalAsh> profile_;
  const raw_ptr<drivefs::DriveFsHost::MountObserver, ExperimentalAsh>
      mount_observer_;

  const DriveFsMojoListenerFactory test_drivefs_mojo_listener_factory_;

  drivefs::DriveFsHost drivefs_host_;

  std::string profile_salt_;

  std::unique_ptr<crosapi::BrowserManager::ScopedKeepAlive>
      native_message_keep_alive_;
  mojo::Remote<crosapi::mojom::DriveFsNativeMessageHostBridge>
      native_message_host_bridge_;
  base::OnceClosure pending_connect_to_extension_request_;
};

// Updates the bulk pinning preference when the `PinManager` stops or errors.
using drivefs::pinning::PinManager;
using drivefs::pinning::Progress;
class DriveIntegrationService::BulkPinningPrefUpdater
    : public PinManager::Observer {
 public:
  explicit BulkPinningPrefUpdater(PrefService* pref_service)
      : pref_service_(pref_service) {
    DCHECK(pref_service_);
  }

  void OnProgress(const Progress& progress) override {
    if (progress.IsError()) {
      pref_service_->SetBoolean(kDriveFsBulkPinningEnabled, false);
      VLOG(1) << "Disabled bulk-pinning because of error " << progress.stage;
    }
  }

 private:
  raw_ptr<PrefService> const pref_service_;
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
          std::move(test_drivefs_mojo_listener_factory))) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile && !profile->IsOffTheRecord());

  logger_ = std::make_unique<EventLogger>();
  blocking_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::WithBaseSyncPrimitives()});

  if (util::IsDriveAvailableForProfile(profile)) {
    preference_watcher_ = std::make_unique<PreferenceWatcher>(profile);
    preference_watcher_->SetIntegrationService(this);
  }

  bool migrated_to_drivefs =
      GetPrefs()->GetBoolean(prefs::kDriveFsPinnedMigrated);
  if (migrated_to_drivefs) {
    state_ = INITIALIZED;
  } else {
    metadata_storage_.reset(new internal::ResourceMetadataStorage(
        cache_root_directory_.Append(kMetadataDirectory),
        blocking_task_runner_.get()));
  }

  SetEnabled(drive::util::IsDriveEnabledForProfile(profile));
}

DriveIntegrationService::~DriveIntegrationService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PrefService* DriveIntegrationService::GetPrefs() const {
  return profile_->GetPrefs();
}

void DriveIntegrationService::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  weak_ptr_factory_.InvalidateWeakPtrs();
  bulk_pinning_pref_sampling_ = false;

  RemoveDriveMountPoint();

  for (auto& observer : observers_) {
    observer.OnDriveIntegrationServiceDestroyed();
  }
}

void DriveIntegrationService::SetEnabled(bool enabled) {
  // If Drive is being disabled, ensure the download destination preference to
  // be out of Drive. Do this before "Do nothing if not changed." because we
  // want to run the check for the first SetEnabled() called in the constructor,
  // which may be a change from false to false.
  if (!enabled) {
    AvoidDriveAsDownloadDirectoryPreference();
  }

  // Do nothing if not changed.
  if (enabled_ == enabled) {
    return;
  }

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

bool DriveIntegrationService::IsOnline() const {
  return preference_watcher_ && preference_watcher_->IsOnline();
}

bool DriveIntegrationService::IsMounted() const {
  if (mount_point_name_.empty()) {
    return false;
  }

  // Look up the registered path, and just discard it.
  // GetRegisteredPath() returns true if the path is available.
  base::FilePath unused;
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);
  return mount_points->GetRegisteredPath(mount_point_name_, &unused);
}

base::FilePath DriveIntegrationService::GetMountPointPath() const {
  return GetDriveFsHost()->GetMountPath();
}

base::FilePath DriveIntegrationService::GetDriveFsLogPath() const {
  return GetDriveFsHost()->GetDataPath().Append("Logs/drivefs.txt");
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

bool DriveIntegrationService::IsSharedDrive(
    const base::FilePath& local_path) const {
  return GetMountPointPath()
      .Append(drive::util::kDriveTeamDrivesDirName)
      .IsParent(local_path);
}

void DriveIntegrationService::AddObserver(Observer* const observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void DriveIntegrationService::RemoveObserver(Observer* const observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

bool DriveIntegrationService::HasObserver(Observer* const observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return observers_.HasObserver(observer);
}

void DriveIntegrationService::ClearCacheAndRemountFileSystem(
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (in_clear_cache_) {
    std::move(callback).Run(false);
    return;
  }
  in_clear_cache_ = true;

  TimeDelta delay;
  if (IsMounted()) {
    RemoveDriveMountPoint();
    // TODO(crbug/1069328): We wait 2 seconds here so that DriveFS can unmount
    // completely. Ideally we'd wait for an unmount complete callback.
    delay = Seconds(2);
  }
  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &DriveIntegrationService::ClearCacheAndRemountFileSystemAfterDelay,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      delay);
}

void DriveIntegrationService::ClearCacheAndRemountFileSystemAfterDelay(
    base::OnceCallback<void(bool)> callback) {
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ClearCache, GetDriveFsHost()->GetDataPath(),
                     GetDriveFsLogPath().DirName()),
      base::BindOnce(
          &DriveIntegrationService::MaybeRemountFileSystemAfterClearCache,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DriveIntegrationService::MaybeRemountFileSystemAfterClearCache(
    base::OnceCallback<void(bool)> callback,
    bool success) {
  if (is_enabled()) {
    AddDriveMountPoint();
  }
  in_clear_cache_ = false;
  std::move(callback).Run(success);
}

drivefs::DriveFsHost* DriveIntegrationService::GetDriveFsHost() const {
  return drivefs_holder_->drivefs_host();
}

DriveFs* DriveIntegrationService::GetDriveFsInterface() const {
  return GetDriveFsHost()->GetDriveFsInterface();
}

void DriveIntegrationService::AddBackDriveMountPoint(
    base::OnceCallback<void(bool)> callback,
    FileError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(callback);

  state_ = error == FILE_ERROR_OK ? INITIALIZED : NOT_INITIALIZED;

  if (error != FILE_ERROR_OK || !enabled_) {
    // Failed to reset, or Drive was disabled during the reset.
    std::move(callback).Run(false);
    return;
  }

  AddDriveMountPoint();
  std::move(callback).Run(true);
}

DriveIntegrationService::DirResult
DriveIntegrationService::EnsureDirectoryExists(const base::FilePath& data_dir) {
  if (base::DirectoryExists(data_dir)) {
    VLOG(1) << "DriveFS data directory '" << data_dir << "' already exists";
    return DirResult::kExisting;
  }

  if (base::CreateDirectory(data_dir)) {
    VLOG(1) << "Created DriveFS data directory '" << data_dir << "'";
    return DirResult::kCreated;
  }

  PLOG(ERROR) << "Cannot create DriveFS data directory '" << data_dir << "'";
  return DirResult::kError;
}

void DriveIntegrationService::AddDriveMountPoint() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZED, state_);
  DCHECK(enabled_);

  weak_ptr_factory_.InvalidateWeakPtrs();
  bulk_pinning_pref_sampling_ = false;

  if (GetDriveFsHost()->IsMounted()) {
    AddDriveMountPointAfterMounted();
    return;
  }

  const bool was_ever_mounted =
      GetPrefs()->GetBoolean(prefs::kDriveFsWasLaunchedAtLeastOnce);

  if (mount_start_.is_null() || was_ever_mounted) {
    mount_start_ = base::TimeTicks::Now();
  }

  const base::FilePath data_dir = GetDriveFsHost()->GetDataPath();
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&EnsureDirectoryExists, data_dir),
      base::BindOnce(&DriveIntegrationService::MaybeMountDrive,
                     weak_ptr_factory_.GetWeakPtr(), data_dir));
}

void DriveIntegrationService::MaybeMountDrive(const base::FilePath& data_dir,
                                              const DirResult data_dir_result) {
  if (data_dir_result == DirResult::kError) {
    return;
  }

  // Check if the data dir was missing (probably because it got removed while
  // the user was logged out).
  if (data_dir_result == DirResult::kCreated &&
      GetPrefs()->GetBoolean(prefs::kDriveFsWasLaunchedAtLeastOnce)) {
    LOG(WARNING) << "DriveFS data directory '" << data_dir
                 << "' went missing and got created again";

    if (util::IsDriveFsBulkPinningEnabled(profile_)) {
      LOG(WARNING)
          << "Displaying system notification and disabling bulk-pinning";

      // Show system notification.
      file_manager::SystemNotificationManager snm(profile_);
      const std::unique_ptr<const message_center::Notification> notification =
          snm.CreateNotification("drive_data_dir_missing",
                                 IDS_FILE_BROWSER_DRIVE_DATA_DIR_MISSING_TITLE,
                                 IDS_FILE_BROWSER_DRIVE_DATA_DIR_MISSING);
      DCHECK(notification);
      snm.GetNotificationDisplayService()->Display(
          NotificationHandler::Type::TRANSIENT, *notification, nullptr);

      // Disable bulk-pinning.
      base::UmaHistogramBoolean(
          "FileBrowser.GoogleDrive.BulkPinning.StateWhenCacheVolumeRemoved",
          GetPrefs()->GetBoolean(kDriveFsBulkPinningEnabled));
      GetPrefs()->SetBoolean(kDriveFsBulkPinningEnabled, false);
    }
  }

  GetDriveFsHost()->Mount();
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
    logger_->Log(logging::LOGGING_INFO, "Drive mount point is added");
    for (auto& observer : observers_) {
      observer.OnFileSystemMounted();
    }
  }

  if (preference_watcher_) {
    preference_watcher_->UpdateSyncPauseState();
  }

  if (!GetPrefs()->GetBoolean(prefs::kDriveFsPinnedMigrated)) {
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
      logger_->Log(logging::LOGGING_INFO, "Drive mount point is removed");
    }
  }
  GetDriveFsHost()->Unmount();

  if (pin_manager_) {
    pin_manager_->Stop();
    pin_manager_->RemoveObserver(this);
    if (bulk_pinning_pref_updater_) {
      pin_manager_->RemoveObserver(bulk_pinning_pref_updater_.get());
      bulk_pinning_pref_updater_.reset();
    }
    GetDriveFsHost()->RemoveObserver(pin_manager_.get());
    pin_manager_.reset();
  }
}

void DriveIntegrationService::MaybeRemountFileSystem(
    absl::optional<TimeDelta> remount_delay,
    bool failed_to_mount) {
  DCHECK_EQ(INITIALIZED, state_);

  RemoveDriveMountPoint();

  if (!remount_delay) {
    if (failed_to_mount && preference_watcher_ &&
        !preference_watcher_->IsOnline()) {
      logger_->Log(logging::LOGGING_WARNING,
                   "DriveFs failed to start; will retry when online");
      remount_when_online_ = true;
      return;
    }
    // If DriveFs didn't specify retry time it's likely unexpected error, e.g.
    // crash. Use limited exponential backoff for retry.
    ++drivefs_consecutive_failures_count_;
    ++drivefs_total_failures_count_;
    if (drivefs_total_failures_count_ > 10) {
      mount_failed_ = true;
      logger_->Log(logging::LOGGING_ERROR,
                   "DriveFs is too crashy. Leaving it alone.");
      RecordBulkPinningMountFailureReason(
          profile_, BulkPinningMountFailureReason::kMoreThanTenTotalFailures);
      for (auto& observer : observers_) {
        observer.OnFileSystemMountFailed();
      }
      return;
    }
    if (drivefs_consecutive_failures_count_ > 3) {
      mount_failed_ = true;
      logger_->Log(logging::LOGGING_ERROR,
                   "DriveFs keeps failing at start. Giving up.");
      RecordBulkPinningMountFailureReason(
          profile_, BulkPinningMountFailureReason::kThreeConsecutiveFailures);
      for (auto& observer : observers_) {
        observer.OnFileSystemMountFailed();
      }
      return;
    }
    remount_delay =
        Seconds(5 * (1 << (drivefs_consecutive_failures_count_ - 1)));
    logger_->Log(logging::LOGGING_WARNING, "DriveFs died, retry in %d seconds",
                 static_cast<int>(remount_delay.value().InSeconds()));
  }

  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DriveIntegrationService::AddDriveMountPoint,
                     weak_ptr_factory_.GetWeakPtr()),
      remount_delay.value());
}

void DriveIntegrationService::OnMounted(const base::FilePath& mount_path) {
  if (AddDriveMountPointAfterMounted()) {
    PrefService* prefs = GetPrefs();
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

  // Enable MirrorSync if the feature is enabled.
  if (ash::features::IsDriveFsMirroringEnabled() &&
      GetPrefs()->GetBoolean(prefs::kDriveFsEnableMirrorSync)) {
    ToggleMirroring(
        true,
        base::BindOnce(&DriveIntegrationService::OnEnableMirroringStatusUpdate,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Enable bulk-pinning if the feature is enabled.
  if (util::IsDriveFsBulkPinningEnabled(profile_)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Instantiate a PinManager.
    DCHECK(!pin_manager_);
    const int max_queue_size =
        GetPrefs()->GetInteger(prefs::kDriveFsBulkPinningMaxQueueSize);
    pin_manager_ = std::make_unique<PinManager>(
        profile_->GetPath(), mount_path, GetDriveFsInterface(), max_queue_size);

    // Listen to progress events from this PinManager.
    pin_manager_->AddObserver(this);
    if (!observers_.empty()) {
      OnProgress(pin_manager_->GetProgress());
    }

    DCHECK(!bulk_pinning_pref_updater_);
    bulk_pinning_pref_updater_ =
        std::make_unique<BulkPinningPrefUpdater>(GetPrefs());
    pin_manager_->AddObserver(bulk_pinning_pref_updater_.get());
    GetDriveFsHost()->AddObserver(pin_manager_.get());

    if (preference_watcher_) {
      pin_manager_->SetOnline(preference_watcher_->IsOnline());
    }

    ToggleBulkPinning();

    if (!bulk_pinning_pref_sampling_) {
      bulk_pinning_pref_sampling_ = true;
      SampleBulkPinningPref();
    }

    RecordBulkPinningMountFailureReason(
        profile_, BulkPinningMountFailureReason::kSuccess);

    for (Observer& observer : observers_) {
      observer.OnBulkPinInitialized();
    }
  }
}

void DriveIntegrationService::SampleBulkPinningPref() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bulk_pinning_pref_sampling_);
  const bool enabled = GetPrefs()->GetBoolean(kDriveFsBulkPinningEnabled);
  VLOG(1) << "Bulk-pinning is currently " << (enabled ? "en" : "dis")
          << "abled";
  base::UmaHistogramBoolean("FileBrowser.GoogleDrive.BulkPinning.Enabled",
                            enabled);
  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DriveIntegrationService::SampleBulkPinningPref,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Hours(1));
}

void DriveIntegrationService::OnUnmounted(
    absl::optional<TimeDelta> remount_delay) {
  UmaEmitUnmountOutcome(remount_delay ? DriveMountStatus::kTemporaryUnavailable
                                      : DriveMountStatus::kUnknownFailure);
  MaybeRemountFileSystem(remount_delay, false);
}

void DriveIntegrationService::OnMountFailed(
    MountFailure failure,
    absl::optional<TimeDelta> remount_delay) {
  PrefService* prefs = GetPrefs();
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

void DriveIntegrationService::OnProgress(
    const drivefs::pinning::Progress& progress) {
  for (auto& observer : observers_) {
    observer.OnBulkPinProgress(progress);
  }
}

void DriveIntegrationService::Initialize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(NOT_INITIALIZED, state_);
  DCHECK(enabled_);

  state_ = INITIALIZING;

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
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
    GetPrefs()->SetBoolean(prefs::kDriveFsPinnedMigrated, true);
    metadata_storage_.reset();
    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CleanupGCacheV1, cache_root_directory_,
                       base::FilePath(),
                       std::vector<std::pair<base::FilePath, std::string>>()));
  }
  state_ = INITIALIZED;
  if (enabled_) {
    AddDriveMountPoint();
  }
}

void DriveIntegrationService::AvoidDriveAsDownloadDirectoryPreference() {
  if (DownloadDirectoryPreferenceIsInDrive()) {
    GetPrefs()->SetFilePath(
        ::prefs::kDownloadDefaultDirectory,
        file_manager::util::GetDownloadsFolderForProfile(profile_));
  }
}

bool DriveIntegrationService::DownloadDirectoryPreferenceIsInDrive() {
  const auto downloads_path =
      GetPrefs()->GetFilePath(::prefs::kDownloadDefaultDirectory);
  const auto* user = ash::ProfileHelper::Get()->GetUserByProfile(profile_);
  return user && user->GetAccountId().HasAccountIdKey() &&
         GetMountPointPath().IsParent(downloads_path);
}

void DriveIntegrationService::MigratePinnedFiles() {
  if (!metadata_storage_) {
    return;
  }

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &GetPinnedAndDirtyFiles, std::move(metadata_storage_),
          cache_root_directory_,
          file_manager::util::GetDownloadsFolderForProfile(profile_)),
      base::BindOnce(&DriveIntegrationService::PinFiles,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveIntegrationService::PinFiles(
    const std::vector<base::FilePath>& files_to_pin) {
  if (!GetDriveFsHost()->IsMounted()) {
    return;
  }

  for (const auto& path : files_to_pin) {
    GetDriveFsInterface()->SetPinned(path, true, base::DoNothing());
  }
  GetPrefs()->SetBoolean(prefs::kDriveFsPinnedMigrated, true);
}

void DriveIntegrationService::ToggleBulkPinning() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!pin_manager_) {
    VLOG(1) << "No bulk-pinning manager";
    return;
  }

  if (GetPrefs()->GetBoolean(kDriveFsBulkPinningEnabled)) {
    pin_manager_->ShouldPin();
    pin_manager_->Start();
  } else {
    pin_manager_->Stop();
  }
}

void DriveIntegrationService::GetTotalPinnedSize(
    base::OnceCallback<void(int64_t)> callback) {
  if (!util::IsDriveFsBulkPinningEnabled(profile_) || !IsMounted() ||
      !GetDriveFsInterface()) {
    std::move(callback).Run(-1);
    return;
  }

  if (base::Time::Now() < last_offline_storage_size_time_ + Seconds(2)) {
    std::move(callback).Run(last_offline_storage_size_result_);
    return;
  }

  GetDriveFsInterface()->GetOfflineFilesSpaceUsage(
      base::BindOnce(&DriveIntegrationService::OnGetOfflineFilesSpaceUsage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DriveIntegrationService::OnGetOfflineFilesSpaceUsage(
    base::OnceCallback<void(int64_t)> callback,
    drive::FileError error,
    int64_t total_size) {
  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot get offline size: " << error;
    std::move(callback).Run(-1);
    return;
  }
  last_offline_storage_size_result_ = total_size;
  last_offline_storage_size_time_ = base::Time::Now();
  std::move(callback).Run(total_size);
}

void DriveIntegrationService::ClearOfflineFiles(
    base::OnceCallback<void(drive::FileError)> callback) {
  if (!util::IsDriveFsBulkPinningEnabled(profile_) || !IsMounted() ||
      !GetDriveFsInterface()) {
    std::move(callback).Run(drive::FILE_ERROR_SERVICE_UNAVAILABLE);
    return;
  }

  GetDriveFsInterface()->ClearOfflineFiles(std::move(callback));
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
          absl::optional<std::vector<drivefs::mojom::QueryItemPtr>>()));
}

void DriveIntegrationService::OnGetQuickAccessItems(
    GetQuickAccessItemsCallback callback,
    drive::FileError error,
    absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
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

void DriveIntegrationService::SearchDriveByFileName(
    std::string query,
    int max_results,
    drivefs::mojom::QueryParameters::SortField sort_field,
    drivefs::mojom::QueryParameters::SortDirection sort_direction,
    drivefs::mojom::QueryParameters::QuerySource query_source,
    SearchDriveByFileNameCallback callback) const {
  if (!GetDriveFsHost()) {
    std::move(callback).Run(drive::FileError::FILE_ERROR_SERVICE_UNAVAILABLE,
                            {});
    return;
  }

  auto drive_query = drivefs::mojom::QueryParameters::New();
  drive_query->title = query;
  drive_query->page_size = max_results;
  drive_query->sort_field = sort_field;
  drive_query->sort_direction = sort_direction;
  drive_query->query_source = query_source;

  auto on_response = base::BindOnce(
      &DriveIntegrationService::OnSearchDriveByFileName,
      weak_ptr_factory_.GetMutableWeakPtr(), std::move(callback));

  GetDriveFsHost()->PerformSearch(
      std::move(drive_query),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(on_response), drive::FileError::FILE_ERROR_ABORT,
          absl::optional<std::vector<drivefs::mojom::QueryItemPtr>>()));
}

void DriveIntegrationService::OnSearchDriveByFileName(
    SearchDriveByFileNameCallback callback,
    drive::FileError error,
    absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  if (error != drive::FILE_ERROR_OK || !items.has_value()) {
    std::move(callback).Run(error, {});
    return;
  }

  std::move(callback).Run(error, std::move(items.value()));
}

void DriveIntegrationService::OnEnableMirroringStatusUpdate(
    drivefs::mojom::MirrorSyncStatus status) {
  mirroring_enabled_ = (status == drivefs::mojom::MirrorSyncStatus::kSuccess);
  if (mirroring_enabled_) {
    for (auto& observer : observers_) {
      observer.OnMirroringEnabled();
    }
  }
}

void DriveIntegrationService::OnDisableMirroringStatusUpdate(
    drivefs::mojom::MirrorSyncStatus status) {
  if (status == drivefs::mojom::MirrorSyncStatus::kSuccess) {
    mirroring_enabled_ = false;
    for (auto& observer : observers_) {
      observer.OnMirroringDisabled();
    }
  }
}

bool DriveIntegrationService::IsMirroringEnabled() {
  return mirroring_enabled_;
}

void DriveIntegrationService::GetMetadata(
    const base::FilePath& local_path,
    DriveFs::GetMetadataCallback callback) {
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
    DriveFs::LocateFilesByItemIdsCallback callback) {
  if (!IsMounted() || !GetDriveFsInterface()) {
    std::move(callback).Run({});
    return;
  }
  GetDriveFsInterface()->LocateFilesByItemIds(item_ids, std::move(callback));
}

void DriveIntegrationService::GetQuotaUsage(
    DriveFs::GetQuotaUsageCallback callback) {
  if (!IsMounted() || !GetDriveFsInterface()) {
    std::move(callback).Run(drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr);
    return;
  }

  GetDriveFsInterface()->GetQuotaUsage(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr));
}

void DriveIntegrationService::GetPooledQuotaUsage(
    DriveFs::GetPooledQuotaUsageCallback callback) {
  if (!IsMounted() || !GetDriveFsInterface()) {
    std::move(callback).Run(drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr);
    return;
  }

  GetDriveFsInterface()->GetPooledQuotaUsage(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr));
}

void DriveIntegrationService::RestartDrive() {
  MaybeRemountFileSystem(TimeDelta(), false);
}

void DriveIntegrationService::SetStartupArguments(
    std::string arguments,
    base::OnceCallback<void(bool)> callback) {
  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->SetStartupArguments(arguments, std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void DriveIntegrationService::GetStartupArguments(
    base::OnceCallback<void(const std::string&)> callback) {
  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->GetStartupArguments(std::move(callback));
  } else {
    std::move(callback).Run("");
  }
}

void DriveIntegrationService::SetTracingEnabled(bool enabled) {
  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->SetTracingEnabled(enabled);
  }
}

void DriveIntegrationService::SetNetworkingEnabled(bool enabled) {
  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->SetNetworkingEnabled(enabled);
  }
}

void DriveIntegrationService::ForcePauseSyncing(bool enabled) {
  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->ForcePauseSyncing(enabled);
  }
}

void DriveIntegrationService::DumpAccountSettings() {
  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->DumpAccountSettings();
  }
}

void DriveIntegrationService::LoadAccountSettings() {
  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->LoadAccountSettings();
  }
}

void DriveIntegrationService::GetThumbnail(const base::FilePath& path,
                                           bool crop_to_square,
                                           GetThumbnailCallback callback) {
  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->GetThumbnail(path, crop_to_square, std::move(callback));
  }
}

void DriveIntegrationService::ToggleMirroring(
    bool enabled,
    DriveFs::ToggleMirroringCallback callback) {
  if (!ash::features::IsDriveFsMirroringEnabled()) {
    std::move(callback).Run(
        drivefs::mojom::MirrorSyncStatus::kFeatureNotEnabled);
    return;
  }

  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->ToggleMirroring(enabled, std::move(callback));
  }
}

void DriveIntegrationService::ToggleSyncForPath(
    const base::FilePath& path,
    drivefs::mojom::MirrorPathStatus status,
    DriveFs::ToggleSyncForPathCallback callback) {
  if (!ash::features::IsDriveFsMirroringEnabled() || !IsMirroringEnabled()) {
    std::move(callback).Run(drive::FILE_ERROR_SERVICE_UNAVAILABLE);
    return;
  }

  if (status == drivefs::mojom::MirrorPathStatus::kStart) {
    blocking_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&base::DirectoryExists, path),
        base::BindOnce(
            &DriveIntegrationService::ToggleSyncForPathIfDirectoryExists,
            weak_ptr_factory_.GetWeakPtr(), path, std::move(callback)));
    return;
  }

  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->ToggleSyncForPath(path, status, std::move(callback));
  }
}

void DriveIntegrationService::ToggleSyncForPathIfDirectoryExists(
    const base::FilePath& path,
    DriveFs::ToggleSyncForPathCallback callback,
    bool exists) {
  if (!exists) {
    std::move(callback).Run(drive::FILE_ERROR_NOT_FOUND);
    return;
  }

  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->ToggleSyncForPath(path, drivefs::mojom::MirrorPathStatus::kStart,
                               std::move(callback));
  }
}

void DriveIntegrationService::GetSyncingPaths(
    DriveFs::GetSyncingPathsCallback callback) {
  if (!ash::features::IsDriveFsMirroringEnabled() || !IsMirroringEnabled()) {
    std::move(callback).Run(drive::FILE_ERROR_SERVICE_UNAVAILABLE, {});
    return;
  }

  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->GetSyncingPaths(std::move(callback));
  }
}

drivefs::SyncState DriveIntegrationService::GetSyncStateForPath(
    const base::FilePath& drive_path) {
  return GetDriveFsHost()->GetSyncStateForPath(drive_path);
}

void DriveIntegrationService::PollHostedFilePinStates() {
  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->PollHostedFilePinStates();
  }
}

void DriveIntegrationService::ForceReSyncFile(const base::FilePath& local_path,
                                              base::OnceClosure callback) {
  base::FilePath drive_path;
  bool is_feature_enabled = ash::features::IsForceReSyncDriveEnabled() &&
                            chromeos::features::IsUploadOfficeToCloudEnabled();
  if (!is_feature_enabled || !IsMounted() || !GetDriveFsInterface() ||
      !GetRelativeDrivePath(local_path, &drive_path)) {
    std::move(callback).Run();
    return;
  }

  GetDriveFsInterface()->UpdateFromPairedDoc(
      drive_path,
      base::BindOnce(&DriveIntegrationService::OnUpdateFromPairedDocComplete,
                     weak_ptr_factory_.GetWeakPtr(), drive_path,
                     std::move(callback)));
}

void DriveIntegrationService::OnUpdateFromPairedDocComplete(
    const base::FilePath& drive_path,
    base::OnceClosure callback,
    drive::FileError error) {
  if (error != drive::FileError::FILE_ERROR_OK) {
    LOG(ERROR) << "Error in UpdateFromPairedDoc: " << error;
    std::move(callback).Run();
    return;
  }

  GetDriveFsInterface()->GetItemFromCloudStore(
      drive_path, base::BindOnce([](drive::FileError error) {
                    LOG_IF(ERROR, error != drive::FileError::FILE_ERROR_OK)
                        << "Error in GetItemFromCloudStore: " << error;
                  }).Then(std::move(callback)));
}

void DriveIntegrationService::ImmediatelyUpload(
    const base::FilePath& path,
    drivefs::mojom::DriveFs::ImmediatelyUploadCallback callback) {
  if (!IsMounted() || !GetDriveFsInterface()) {
    std::move(callback).Run(drive::FILE_ERROR_SERVICE_UNAVAILABLE);
    return;
  }

  GetDriveFsInterface()->ImmediatelyUpload(path, std::move(callback));
}

void DriveIntegrationService::GetReadOnlyAuthenticationToken(
    GetReadOnlyAuthenticationTokenCallback callback) {
  if (!auth_service_) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_);
    // This class doesn't care about browser sync consent.
    const CoreAccountId& account_id =
        identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

    std::vector<std::string> scopes = {
        GaiaConstants::kDriveReadOnlyOAuth2Scope};
    auth_service_ = std::make_unique<google_apis::AuthService>(
        identity_manager, account_id, profile_->GetURLLoaderFactory(), scopes);
  }

  auth_service_->StartAuthentication(std::move(callback));
}

PinManager* DriveIntegrationService::GetPinManager() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pin_manager_.get();
}

void DriveIntegrationService::RegisterDriveFsNativeMessageHostBridge(
    mojo::PendingRemote<crosapi::mojom::DriveFsNativeMessageHostBridge>
        bridge) {
  drivefs_holder_->RegisterDriveFsNativeMessageHostBridge(std::move(bridge));
}

void DriveIntegrationService::GetDocsOfflineStats(
    DriveFs::GetDocsOfflineStatsCallback callback) {
  if (!IsMounted() || !GetDriveFsInterface()) {
    std::move(callback).Run(drive::FILE_ERROR_SERVICE_UNAVAILABLE,
                            drivefs::mojom::DocsOfflineStats::New());
    return;
  }

  GetDriveFsInterface()->GetDocsOfflineStats(std::move(callback));
}

void DriveIntegrationService::UpdateNetworkState(bool pause_syncing,
                                                 bool is_offline) {
  if (DriveFs* const drivefs = GetDriveFsInterface()) {
    drivefs->UpdateNetworkState(pause_syncing, is_offline);
  }

  util::ConnectionStatusType connection_status =
      util::GetDriveConnectionStatus(profile_);
  for (auto& observer : observers_) {
    observer.OnDriveConnectionStatusChanged(connection_status);
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
  if (!profile) {  // crbug.com/1254581
    return nullptr;
  }
  return static_cast<DriveIntegrationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
DriveIntegrationServiceFactory* DriveIntegrationServiceFactory::GetInstance() {
  return base::Singleton<DriveIntegrationServiceFactory>::get();
}

DriveIntegrationServiceFactory::DriveIntegrationServiceFactory()
    : ProfileKeyedServiceFactory(
          "DriveIntegrationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(DriveNotificationManagerFactory::GetInstance());
  DependsOn(DownloadCoreServiceFactory::GetInstance());
}

DriveIntegrationServiceFactory::~DriveIntegrationServiceFactory() = default;

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
