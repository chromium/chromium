// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DRIVE_DRIVE_INTEGRATION_SERVICE_H_
#define CHROME_BROWSER_ASH_DRIVE_DRIVE_INTEGRATION_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "chromeos/ash/components/drivefs/drivefs_pinning_manager.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/ash/components/drivefs/mojom/notifications.mojom.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_errors.h"
#include "components/drive/file_system_core_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/auth_service_interface.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace drivefs {
class DriveFsHost;
class DriveFsSearchQuery;

namespace mojom {
class DriveFs;
}  // namespace mojom
}  // namespace drivefs

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace drive {

namespace internal {
class ResourceMetadataStorage;
}  // namespace internal

// Mounting status. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class DriveMountStatus {
  kSuccess = 0,
  kUnknownFailure = 1,
  kTemporaryUnavailable = 2,
  kInvocationFailure = 3,
  kUnexpectedDisconnect = 4,
  kTimeout = 5,
  kMaxValue = kTimeout,
};

struct QuickAccessItem {
  base::FilePath path;
  double confidence;
};

// Notifications/Errors coming from DriveFs side which we need to persist in
// the Chrome side.
struct PersistedMessage {
  // Where does the message come from in DriveFs.
  enum Source {
    kNotification = 0,
    kError = 1,
  };
  Source source;

  // DriveFs Notification/Error types which require persistence.
  using Type = absl::variant<drivefs::mojom::DriveFsNotification::Tag,
                             drivefs::mojom::MirrorSyncError::Type>;
  Type type;

  base::FilePath path;

  int64_t stable_id;
};

// DriveIntegrationService is used to integrate Drive to Chrome. This class
// exposes the file system representation built on top of Drive and some
// other Drive related objects to the file manager, and some other sub
// systems.
//
// The class is essentially a container that manages lifetime of the objects
// that are used to integrate Drive to Chrome. The object of this class is
// created per-profile.
class DriveIntegrationService : public KeyedService,
                                public drivefs::DriveFsHost::MountObserver,
                                drivefs::pinning::PinningManager::Observer,
                                ash::NetworkStateHandler::Observer {
 public:
  using DriveFsMojoListenerFactory = base::RepeatingCallback<
      std::unique_ptr<drivefs::DriveFsBootstrapListener>()>;
  using GetQuickAccessItemsCallback =
      base::OnceCallback<void(FileError, std::vector<QuickAccessItem>)>;
  using SearchDriveByFileNameCallback =
      drivefs::mojom::SearchQuery::GetNextPageCallback;
  using GetThumbnailCallback =
      base::OnceCallback<void(const std::optional<std::vector<uint8_t>>&)>;
  using GetReadOnlyAuthenticationTokenCallback =
      base::OnceCallback<void(google_apis::ApiErrorCode code,
                              const std::string& access_token)>;

  // test_mount_point_name, test_cache_root and
  // test_drivefs_mojo_listener_factory are used by tests to inject customized
  // instances.
  // Pass NULL or the empty value when not interested.
  DriveIntegrationService(
      Profile* profile,
      const std::string& test_mount_point_name,
      const base::FilePath& test_cache_root,
      DriveFsMojoListenerFactory test_drivefs_mojo_listener_factory = {});

  DriveIntegrationService(const DriveIntegrationService&) = delete;
  DriveIntegrationService& operator=(const DriveIntegrationService&) = delete;

  ~DriveIntegrationService() override;

  // KeyedService override.
  void Shutdown() override;

  void SetEnabled(bool enabled);
  bool is_enabled() const { return enabled_; }

  bool IsOnline() const { return is_online_; }

  bool IsMounted() const;

  bool mount_failed() const { return mount_failed_; }

  // Returns the path of the mount point for drive. It is only valid to call if
  // |IsMounted()|.
  base::FilePath GetMountPointPath() const;

  // Returns the path of DriveFS log if enabled or empty path.
  base::FilePath GetDriveFsLogPath() const;

  // Returns the path of the DriveFs content cache.
  base::FilePath GetDriveFsContentCachePath() const;

  // Returns true if |local_path| resides inside |GetMountPointPath()|.
  // In this case |drive_path| will contain 'drive' path of this file, e.g.
  // reparented to the mount point.
  // It is only valid to call if |IsMounted()|.
  bool GetRelativeDrivePath(const base::FilePath& local_path,
                            base::FilePath* drive_path) const;

  bool IsSharedDrive(const base::FilePath& local_path) const;

  // Base class for classes that need to observe events from
  // DriveIntegrationService. All events are notified on the UI thread.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    // Triggered when the `DriveIntegrationService` is being destroyed.
    virtual void OnDriveIntegrationServiceDestroyed() {}

    // Triggered when the file system is mounted.
    virtual void OnFileSystemMounted() {}

    // Triggered when the file system is being unmounted.
    virtual void OnFileSystemBeingUnmounted() {}

    // Triggered when mounting the filesystem has failed in a fashion that will
    // not be automatically retried.
    virtual void OnFileSystemMountFailed() {}

    // Triggered when the mirroring functionality is enabled.
    virtual void OnMirroringEnabled() {}

    // Triggered when the mirroring functionality is disabled.
    virtual void OnMirroringDisabled() {}

    // Triggered when the bulk pinning manager reports progress.
    virtual void OnBulkPinProgress(const drivefs::pinning::Progress& progress) {
    }

    // Triggered when the bulk-pinning manager is fully initialized.
    virtual void OnBulkPinInitialized() {}

    // Triggered when the network connection to Drive could have changed.
    virtual void OnDriveConnectionStatusChanged(util::ConnectionStatus status) {
    }

    // Starts observing the given service.
    void Observe(DriveIntegrationService* service);

    // Stops observing the service.
    void Reset();

    // Gets a pointer to the service being observed.
    DriveIntegrationService* GetService() const { return service_; }

   private:
    // The service being observed.
    raw_ptr<DriveIntegrationService> service_ = nullptr;
  };

  // MountObserver implementation.
  void OnMounted(const base::FilePath& mount_path) override;
  void OnUnmounted(std::optional<base::TimeDelta> remount_delay) override;
  void OnMountFailed(MountFailure failure,
                     std::optional<base::TimeDelta> remount_delay) override;

  // PinningManager::Observer implementation
  using Progress = drivefs::pinning::Progress;
  void OnProgress(const Progress& progress) override;

  EventLogger* GetLogger() { return &logger_; }

  // Clears all the local cache folder and remounts the file system. |callback|
  // is called with true when this operation is done successfully. Otherwise,
  // |callback| is called with false. |callback| must not be null.
  void ClearCacheAndRemountFileSystem(base::OnceCallback<void(bool)> callback);

  // Returns the DriveFsHost if it is enabled.
  drivefs::DriveFsHost* GetDriveFsHost() const;

  // Returns the PinningManager if DriveFS is mounted and bulk-pinning is
  // enabled.
  using PinningManager = drivefs::pinning::PinningManager;
  PinningManager* GetPinningManager() const;

  // Returns the mojo interface to the DriveFs daemon if it is enabled and
  // connected.
  drivefs::mojom::DriveFs* GetDriveFsInterface() const;

  void GetQuickAccessItems(int max_number,
                           GetQuickAccessItemsCallback callback);

  void SearchDriveByFileName(
      std::string query,
      int max_results,
      drivefs::mojom::QueryParameters::SortField sort_field,
      drivefs::mojom::QueryParameters::SortDirection sort_direction,
      drivefs::mojom::QueryParameters::QuerySource query_source,
      SearchDriveByFileNameCallback callback) const;
  // Returns nullptr if DriveFS is not mounted.
  std::unique_ptr<drivefs::DriveFsSearchQuery> CreateSearchQueryByFileName(
      std::string query,
      int max_results,
      drivefs::mojom::QueryParameters::SortField sort_field,
      drivefs::mojom::QueryParameters::SortDirection sort_direction,
      drivefs::mojom::QueryParameters::QuerySource query_source) const;

  // Returns the metadata for Drive file at |local_path|.
  void GetMetadata(const base::FilePath& local_path,
                   drivefs::mojom::DriveFs::GetMetadataCallback callback);

  // Locates files or dirs by their server-side ID. Paths are relative to the
  // mount point.
  void LocateFilesByItemIds(
      const std::vector<std::string>& item_ids,
      drivefs::mojom::DriveFs::LocateFilesByItemIdsCallback callback);

  // Returns the total and free space available in the user's Drive.
  void GetQuotaUsage(drivefs::mojom::DriveFs::GetQuotaUsageCallback callback);

  // Returns the total and free space available in the user's Drive.
  // Additionally, if the user belongs to an organization, whether the
  // organization quota is full or not, and the name of the organization.
  void GetPooledQuotaUsage(
      drivefs::mojom::DriveFs::GetPooledQuotaUsageCallback callback);

  void RestartDrive();

  // Sets the arguments to be parsed by DriveFS on startup. Should only be
  // called in developer mode.
  void SetStartupArguments(std::string arguments,
                           base::OnceCallback<void(bool)> callback);

  // Gets the currently set arguments parsed by DriveFS on startup. Should only
  // be called in developer mode.
  void GetStartupArguments(
      base::OnceCallback<void(const std::string&)> callback);

  // Enables or disables performance tracing, which logs to
  // |data_dir_path|/Logs/drive_fs_trace.
  void SetTracingEnabled(bool enabled);

  // Enables or disables networking for testing. Should only be called in
  // developer mode.
  void SetNetworkingEnabled(bool enabled);

  // Overrides syncing to be paused if enabled. Should only be called in
  // developer mode.
  void ForcePauseSyncing(bool enabled);

  // Dumps account settings (including feature flags) to
  // |data_dir_path/account_settings. Should only be called in developer mode.
  void DumpAccountSettings();

  // Loads account settings (including feature flags) from
  // |data_dir_path/account_settings. Should only be called in developer mode.
  void LoadAccountSettings();

  // Returns a PNG containing a thumbnail for |path|. If |crop_to_square|, a
  // 360x360 thumbnail, cropped to fit a square is returned; otherwise a
  // thumbnail up to 500x500, maintaining aspect ration, is returned. If |path|
  // does not exist or does not have a thumbnail, |thumbnail| will be null.
  void GetThumbnail(const base::FilePath& path,
                    bool crop_to_square,
                    GetThumbnailCallback callback);

  // Toggle mirroring on or off defined by |enabled|.
  void ToggleMirroring(
      bool enabled,
      drivefs::mojom::DriveFs::ToggleMirroringCallback callback);

  // Toggle syncing for a specific path. Should only be called once mirroring
  // has been enabled via |ToggleMirroring|.
  void ToggleSyncForPath(
      const base::FilePath& path,
      drivefs::mojom::MirrorPathStatus status,
      drivefs::mojom::DriveFs::ToggleSyncForPathCallback callback);

  // Retrieves a list of paths being synced.
  void GetSyncingPaths(
      drivefs::mojom::DriveFs::GetSyncingPathsCallback callback);

  // Tells DriveFS to update its cached pin states of hosted files (once).
  void PollHostedFilePinStates();

  // Returns whether mirroring is enabled.
  bool IsMirroringEnabled();

  // Requests Drive to resync the office file at |local_path| from the cloud.
  void ForceReSyncFile(const base::FilePath& local_path,
                       base::OnceClosure callback);

  // Gets a read-only OAuth token that allows downloading files from the user's
  // Drive. If an error occurs or the user does not have access to download
  // files from Drive, `access_token` will be an empty string.
  void GetReadOnlyAuthenticationToken(
      GetReadOnlyAuthenticationTokenCallback callback);

  // Returns via callback the amount of storage taken by all currently pinned
  // files.
  void GetTotalPinnedSize(base::OnceCallback<void(int64_t)> callback);

  void ClearOfflineFiles(base::OnceCallback<void(FileError)> callback);

  // Tells Drive to immediately start uploading the file at |path|, which is a
  // relative path in Drive. This avoids queuing delays for newly created files,
  // when we are sure that there are no more subsequent operations on the file
  // that we should wait for.
  void ImmediatelyUpload(
      const base::FilePath& path,
      drivefs::mojom::DriveFs::ImmediatelyUploadCallback callback);

  // Called by lacros to register a bridge that this service can call into when
  // DriveFS wants to initiate a connection to an extension in lacros.
  void RegisterDriveFsNativeMessageHostBridge(
      mojo::PendingRemote<crosapi::mojom::DriveFsNativeMessageHostBridge>
          bridge);

  // Gets counts of files in docs offline extension.
  void GetDocsOfflineStats(
      drivefs::mojom::DriveFs::GetDocsOfflineStatsCallback callback);

  // Gets the mirror sync status for a specific file.
  void GetMirrorSyncStatusForFile(
      const base::FilePath& path,
      drivefs::mojom::DriveFs::GetMirrorSyncStatusForFileCallback callback);

  // Gets the mirror sync status for a specific directory.
  void GetMirrorSyncStatusForDirectory(
      const base::FilePath& path,
      drivefs::mojom::DriveFs::GetMirrorSyncStatusForDirectoryCallback
          callback);

  void OnNetworkChanged();

  // Register the drive related profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* prefs);

 private:
  enum class State {
    kNone,
    kInitializing,
    kInitialized,
    kRemounting,
  };

  class DriveFsHolder;

  PrefService* GetPrefs() const { return profile_->GetPrefs(); }

  // Returns true if Drive is enabled.
  // Must be called on UI thread.
  bool IsDriveEnabled();

  enum class DirResult { kError, kExisting, kCreated };
  static DirResult EnsureDirectoryExists(const base::FilePath& data_dir);

  // Registers remote file system for drive mount point. If DriveFS is enabled,
  // but not yet mounted, this will start it mounting and wait for it to
  // complete before adding the mount point.
  void AddDriveMountPoint();

  // Mounts Drive if the directory exists.
  void MaybeMountDrive(const base::FilePath& data_dir,
                       DirResult data_dir_result);

  // Registers remote file system for drive mount point.
  bool AddDriveMountPointAfterMounted();

  // Unregisters drive mount point from File API.
  void RemoveDriveMountPoint();

  // Adds back the drive mount point.
  // Used to implement ClearCacheAndRemountFileSystem().
  void AddBackDriveMountPoint(base::OnceCallback<void(bool)> callback,
                              FileError error);

  // Unregisters drive mount point, and if |remount_delay| is specified
  // then tries to add it back after that delay. If |remount_delay| isn't
  // specified, |failed_to_mount| is true and the user is offline, schedules a
  // retry when the user is online.
  void MaybeRemountFileSystem(std::optional<base::TimeDelta> remount_delay,
                              bool failed_to_mount);

  // Helper function for ClearCacheAndRemountFileSystem() that deletes the cache
  // folder.
  void ClearCacheAndRemountFileSystemAfterDelay(
      base::OnceCallback<void(bool)> callback);

  // Helper function for ClearCacheAndRemountFileSystem() that remounts Drive if
  // necessary.
  void MaybeRemountFileSystemAfterClearCache(
      base::OnceCallback<void(bool)> callback,
      bool success);

  // Initializes the object. This function should be called before any
  // other functions.
  void Initialize();

  // Called when metadata initialization is done. Continues initialization if
  // the metadata initialization is successful.
  void InitializeAfterMetadataInitialized(FileError error);

  // Change the download directory to the local "Downloads" if the download
  // destination is set under Drive. This must be called when disabling Drive.
  void AvoidDriveAsDownloadDirectoryPreference();

  bool DownloadDirectoryPreferenceIsInDrive();

  // Migrate pinned files from the old Drive integration to DriveFS.
  void MigratePinnedFiles();

  // Pin all the files in |files_to_pin| with DriveFS.
  void PinFiles(const std::vector<base::FilePath>& files_to_pin);

  // Called when the "drivefs.bulk_pinning_enabled" pref changes value.
  // Starts or stops DriveFS bulk pinning accordingly.
  // Does nothing if there is no bulk-pinning manager.
  void StartOrStopBulkPinning();

  // Called when the "drivefs.bulk_pinning.visible" pref changes value.
  // Creates or deletes the DriveFS bulk-pinning manager accordingly.
  void CreateOrDeleteBulkPinningManager();

  // Regularly samples the bulk-pinning preference and stores the result in a
  // UMA histogram.
  void SampleBulkPinningPref();

  void OnGetOfflineItemsPage(
      int64_t total_size,
      mojo::Remote<drivefs::mojom::SearchQuery> search_query,
      base::OnceCallback<void(int64_t)> callback,
      FileError error,
      std::optional<std::vector<drivefs::mojom::QueryItemPtr>> results);

  void OnGetQuickAccessItems(
      GetQuickAccessItemsCallback callback,
      FileError error,
      std::optional<std::vector<drivefs::mojom::QueryItemPtr>> items);

  void OnSearchDriveByFileName(
      SearchDriveByFileNameCallback callback,
      FileError error,
      std::optional<std::vector<drivefs::mojom::QueryItemPtr>> items);

  void OnEnableMirroringStatusUpdate(drivefs::mojom::MirrorSyncStatus status);
  void OnMyFilesSyncPathAdded(drive::FileError status);

  void OnDisableMirroringStatusUpdate(drivefs::mojom::MirrorSyncStatus status);

  // Before adding a new root, get all existing roots first to see if it exists
  // or not. If it exists, do nothing.
  void OnGetSyncPathsForAddingPath(
      const base::FilePath& path_to_add,
      drivefs::mojom::DriveFs::ToggleSyncForPathCallback callback,
      drive::FileError status,
      const std::vector<base::FilePath>& paths);

  // Toggle syncing for |path| if the the directory exists.
  void ToggleSyncForPathIfDirectoryExists(
      const base::FilePath& path,
      drivefs::mojom::DriveFs::ToggleSyncForPathCallback callback,
      bool exists);

  void OnUpdateFromPairedDocComplete(const base::FilePath& drive_path,
                                     base::OnceClosure callback,
                                     FileError error);

  void OnGetOfflineFilesSpaceUsage(base::OnceCallback<void(int64_t)> callback,
                                   FileError error,
                                   int64_t total_size);

  void RegisterPrefs();
  void OnDrivePrefChanged();
  void OnMirroringPrefChanged();

  // NetworkStateHandler::Observer implementation.
  void PortalStateChanged(const ash::NetworkState*,
                          ash::NetworkState::PortalState portal_state) override;
  void DefaultNetworkChanged(const ash::NetworkState*) override;
  void OnShuttingDown() override;

  friend class DriveIntegrationServiceFactory;

  const raw_ptr<Profile> profile_;

  State state_ = State::kNone;
  bool enabled_ = false;
  bool mount_failed_ = false;
  bool in_clear_cache_ = false;

  // Is the bulk-pinning preference sampling task currently scheduled?
  bool bulk_pinning_pref_sampling_ = false;
  bool mirroring_enabled_ = false;
  bool is_online_ = true;
  bool remount_when_online_ = false;

  // Custom mount point name that can be injected for testing in constructor.
  std::string mount_point_name_;
  base::FilePath cache_root_directory_;
  EventLogger logger_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unique_ptr<internal::ResourceMetadataStorage, util::DestroyHelper>
      metadata_storage_;

  base::ObserverList<Observer, true> observers_;

  std::unique_ptr<DriveFsHolder> drivefs_holder_;

  std::unique_ptr<PinningManager> pinning_manager_;

  int drivefs_total_failures_count_ = 0;
  int drivefs_consecutive_failures_count_ = 0;

  // Used to fetch authentication and refresh tokens from Drive.
  std::unique_ptr<google_apis::AuthServiceInterface> auth_service_;

  base::TimeTicks mount_start_;

  base::Time last_offline_storage_size_time_;
  int64_t last_offline_storage_size_result_;

  PrefChangeRegistrar registrar_;

  base::ScopedObservation<ash::NetworkStateHandler,
                          ash::NetworkStateHandler::Observer>
      network_state_handler_{this};

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveIntegrationService> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(DriveIntegrationServiceTest, EnsureDirectoryExists);
};

// Singleton that owns all instances of DriveIntegrationService and
// associates them with Profiles.
class DriveIntegrationServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Factory function used by tests.
  using FactoryCallback =
      base::RepeatingCallback<DriveIntegrationService*(Profile* profile)>;

  // Sets and resets a factory function for tests. See below for why we can't
  // use BrowserContextKeyedServiceFactory::SetTestingFactory().
  class ScopedFactoryForTest {
   public:
    explicit ScopedFactoryForTest(FactoryCallback* factory_for_test);
    ~ScopedFactoryForTest();
  };

  // Returns the DriveIntegrationService for |profile|, creating it if it is
  // not yet created.
  static DriveIntegrationService* GetForProfile(Profile* profile);

  // Returns the DriveIntegrationService that is already associated with
  // |profile|, if it is not yet created it will return NULL.
  static DriveIntegrationService* FindForProfile(Profile* profile);

  // Returns the DriveIntegrationServiceFactory instance.
  static DriveIntegrationServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<DriveIntegrationServiceFactory>;

  DriveIntegrationServiceFactory();
  ~DriveIntegrationServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  // This is static so it can be set without instantiating the factory. This
  // allows factory creation to be delayed until it normally happens (on profile
  // creation) rather than when tests are set up. DriveIntegrationServiceFactory
  // transitively depends on ExtensionSystemFactory which crashes if created too
  // soon (i.e. before the BrowserProcess exists).
  static FactoryCallback* factory_for_test_;
};

}  // namespace drive

#endif  // CHROME_BROWSER_ASH_DRIVE_DRIVE_INTEGRATION_SERVICE_H_
