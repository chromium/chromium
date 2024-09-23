// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/components/arc/session/arc_service_manager.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/device_event_router.h"
#include "chrome/browser/ash/extensions/file_manager/drivefs_event_router.h"
#include "chrome/browser/ash/extensions/file_manager/office_tasks.h"
#include "chrome/browser/ash/extensions/file_manager/system_notification_manager.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "chrome/browser/ash/file_manager/file_watcher.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"
#include "chrome/browser/ash/policy/skyvault/local_user_files_policy_observer.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "extensions/browser/extension_registry_observer.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "ui/display/display_observer.h"
#include "url/gurl.h"
#include "url/origin.h"

class PrefChangeRegistrar;
class Profile;

using OutputsType =
    extensions::api::file_manager_private::ProgressStatus::OutputsType;
using file_manager::util::EntryDefinition;

namespace display {
enum class TabletState;
}  // namespace display

namespace file_manager {

// Monitors changes in disk mounts, network connection state and preferences
// affecting File Manager. Dispatches appropriate File Browser events.
class EventRouter
    : public KeyedService,
      extensions::ExtensionRegistryObserver,
      ash::system::TimezoneSettings::Observer,
      VolumeManagerObserver,
      arc::ArcIntentHelperObserver,
      drive::DriveIntegrationService::Observer,
      guest_os::GuestOsSharePath::Observer,
      display::DisplayObserver,
      file_manager::io_task::IOTaskController::Observer,
      guest_os::GuestOsMountProviderRegistry::Observer,
      chromeos::DlpClient::Observer,
      apps::AppRegistryCache::Observer,
      network::NetworkConnectionTracker::NetworkConnectionObserver,
      policy::local_user_files::LocalUserFilesPolicyObserver {
 public:
  using DispatchDirectoryChangeEventImplCallback =
      base::RepeatingCallback<void(const base::FilePath& virtual_path,
                                   bool got_error,
                                   const std::vector<url::Origin>& listeners)>;

  explicit EventRouter(Profile* profile);

  EventRouter(const EventRouter&) = delete;
  EventRouter& operator=(const EventRouter&) = delete;

  ~EventRouter() override;

  // arc::ArcIntentHelperObserver overrides.
  void OnIntentFiltersUpdated(
      const std::optional<std::string>& package_name) override;

  // KeyedService overrides.
  void Shutdown() override;

  using BoolCallback = base::OnceCallback<void(bool success)>;

  // Adds a file watch at |local_path|, associated with |virtual_path|, for
  // an listener with |listener_origin|.
  //
  // |callback| will be called with true on success, or false on failure.
  // |callback| must not be null.
  //
  // Obsolete. Used as fallback for files which backends do not implement the
  // storage::WatcherManager interface.
  void AddFileWatch(const base::FilePath& local_path,
                    const base::FilePath& virtual_path,
                    const url::Origin& listener_origin,
                    BoolCallback callback);

  // Removes a file watch at |local_path| for listener with |listener_origin|.
  //
  // Obsolete. Used as fallback for files which backends do not implement the
  // storage::WatcherManager interface.
  void RemoveFileWatch(const base::FilePath& local_path,
                       const url::Origin& listener_origin);

  // Called when a notification from a watcher manager arrives.
  void OnWatcherManagerNotification(
      const storage::FileSystemURL& file_system_url,
      const url::Origin& listener_origin,
      storage::WatcherManager::ChangeType change_type);

  // extensions::ExtensionRegistryObserver overrides
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // ash::system::TimezoneSettings::Observer overrides.
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // VolumeManagerObserver overrides.
  void OnDiskAdded(const ash::disks::Disk& disk, bool mounting) override;
  void OnDiskRemoved(const ash::disks::Disk& disk) override;
  void OnDeviceAdded(const std::string& device_path) override;
  void OnDeviceRemoved(const std::string& device_path) override;
  void OnVolumeMounted(ash::MountError error_code,
                       const Volume& volume) override;
  void OnVolumeUnmounted(ash::MountError error_code,
                         const Volume& volume) override;
  void OnFormatStarted(const std::string& device_path,
                       const std::string& device_label,
                       bool success) override;
  void OnFormatCompleted(const std::string& device_path,
                         const std::string& device_label,
                         bool success) override;
  void OnPartitionStarted(const std::string& device_path,
                          const std::string& device_label,
                          bool success) override;
  void OnPartitionCompleted(const std::string& device_path,
                            const std::string& device_label,
                            bool success) override;
  void OnRenameStarted(const std::string& device_path,
                       const std::string& device_label,
                       bool success) override;
  void OnRenameCompleted(const std::string& device_path,
                         const std::string& device_label,
                         bool success) override;
  // Set custom dispatch directory change event implementation for testing.
  void SetDispatchDirectoryChangeEventImplForTesting(
      const DispatchDirectoryChangeEventImplCallback& callback);

  // DriveIntegrationService::Observer implementation.
  void OnFileSystemMountFailed() override;
  void OnDriveConnectionStatusChanged(
      drive::util::ConnectionStatus status) override;

  // GuestOsSharePath::Observer implementation.
  void OnPersistedPathRegistered(const std::string& vm_name,
                                 const base::FilePath& path) override;
  void OnUnshare(const std::string& vm_name,
                 const base::FilePath& path) override;
  void OnGuestRegistered(const guest_os::GuestId& guest) override;
  void OnGuestUnregistered(const guest_os::GuestId& guest) override;

  // display::DisplayObserver overrides.
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // Notifies FilesApp that file drop to Plugin VM was not in a shared directory
  // and failed FilesApp will show the "Move to Windows files" dialog.
  void DropFailedPluginVmDirectoryNotShared();

  // Called by the UI to notify the result of a displayed dialog.
  void OnDriveDialogResult(drivefs::mojom::DialogResult result);

  // Returns a weak pointer for the event router.
  base::WeakPtr<EventRouter> GetWeakPtr();

  // IOTaskController::Observer:
  void OnIOTaskStatus(const io_task::ProgressStatus& status) override;

  // guest_os::GuestOsMountProviderRegistry::Observer overrides.
  void OnRegistered(guest_os::GuestOsMountProviderRegistry::Id id,
                    guest_os::GuestOsMountProvider* provider) override;
  void OnUnregistered(guest_os::GuestOsMountProviderRegistry::Id id) override;

  // Broadcast to Files app frontend that file tasks might have changed.
  void BroadcastOnAppsUpdatedEvent();

  drivefs::SyncState GetDriveSyncStateForPath(const base::FilePath& drive_path);

  // chromeos::DlpClient::Observer override.
  void OnFilesAddedToDlpDaemon(
      const std::vector<base::FilePath>& files) override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(const network::mojom::ConnectionType type) override;

  // policy::local_user_files::Observer:
  void OnLocalUserFilesPolicyChanged() override;

  // Records that there's a `CloudOpenTask` for the `file_url`.
  bool AddCloudOpenTask(const storage::FileSystemURL& file_url);
  // Removes the record of a `CloudOpenTask` for the `file_url`.
  void RemoveCloudOpenTask(const storage::FileSystemURL& file_url);

  // Use this method for unit tests to bypass checking if there are any SWA
  // windows.
  void ForceBroadcastingForTesting(bool enabled) {
    force_broadcasting_for_testing_ = enabled;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(EventRouterTest, PopulateCrostiniEvent);
  friend class ScopedSuppressDriveNotificationsForPath;

  // Starts observing file system change events.
  void ObserveEvents();

  // Called when prefs related to file manager change.
  void OnFileManagerPrefsChanged();

  // Process file watch notifications.
  void HandleFileWatchNotification(const base::FilePath& path, bool got_error);

  // Sends directory change event.
  void DispatchDirectoryChangeEvent(const base::FilePath& path,
                                    bool got_error,
                                    const std::vector<url::Origin>& listeners);

  // Default implementation of DispatchDirectoryChangeEvent.
  void DispatchDirectoryChangeEventImpl(
      const base::FilePath& path,
      bool got_error,
      const std::vector<url::Origin>& listeners);

  // Sends directory change event, after converting the file definition to entry
  // definition.
  void DispatchDirectoryChangeEventWithEntryDefinition(
      bool watcher_error,
      const EntryDefinition& entry_definition);

  // Dispatches the mount completed event.
  void DispatchMountCompletedEvent(
      extensions::api::file_manager_private::MountCompletedEventType event_type,
      ash::MountError error,
      const Volume& volume);

  // Send crostini path shared or unshared event.
  void SendCrostiniEvent(
      extensions::api::file_manager_private::CrostiniEventType event_type,
      const std::string& vm_name,
      const base::FilePath& path);

  // Populate the crostini path shared or unshared event.
  static void PopulateCrostiniEvent(
      extensions::api::file_manager_private::CrostiniEvent& event,
      extensions::api::file_manager_private::CrostiniEventType event_type,
      const std::string& vm_name,
      const url::Origin& origin,
      const std::string& mount_name,
      const std::string& file_system_name,
      const std::string& full_path);

  void NotifyDriveConnectionStatusChanged();

  // Used by `file_manager::ScopedSuppressDriveNotificationsForPath` to prevent
  // Drive notifications for a given file identified by its relative Drive path.
  void SuppressDriveNotificationsForFilePath(
      const base::FilePath& relative_drive_path);
  void RestoreDriveNotificationsForFilePath(
      const base::FilePath& relative_drive_path);

  // Called to refresh the list of guests and broadcast it.
  void OnMountableGuestsChanged();

  // After resolving all file definitions, ensure they are available on the
  // `event_status`.
  void OnConvertFileDefinitionListToEntryDefinitionList(
      file_manager_private::ProgressStatus event_status,
      std::unique_ptr<file_manager::util::EntryDefinitionList>
          entry_definition_list);

  // Notifies Files app frontend that some files have changed.
  void OnFilesChanged(
      const std::vector<base::FilePath>& files,
      extensions::api::file_manager_private::ChangeType change_type);

  // Broadcast a directory change event for directories and files in
  // `files_to_directory_map`.
  void BroadcastDirectoryChangeEvent(
      const std::map<base::FilePath, std::vector<base::FilePath>>&
          files_to_directory_map,
      const GURL& listener_url,
      extensions::api::file_manager_private::ChangeType change_type);

  // Broadcast a directory change event for the files listed in `changed_files`
  // belonging to a filesystem described by `info`.
  void BroadcastDirectoryChangeEventOnFilesystemInfoResolved(
      GURL listener_url,
      std::vector<base::FilePath> changed_files,
      extensions::api::file_manager_private::ChangeType change_type,
      base::File::Error result,
      const storage::FileSystemInfo& info,
      const base::FilePath& dir_path,
      storage::FileSystemContext::ResolvedEntryType);

  // Broadcast the `event_status` to all open SWA windows.
  void BroadcastIOTask(
      const file_manager_private::ProgressStatus& event_status);

  std::map<base::FilePath, std::unique_ptr<FileWatcher>> file_watchers_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  raw_ptr<Profile> profile_;

  std::unique_ptr<SystemNotificationManager> notification_manager_;
  std::unique_ptr<OfficeTasks> office_tasks_;
  std::unique_ptr<DeviceEventRouter> device_event_router_;
  const std::unique_ptr<DriveFsEventRouter> drivefs_event_router_;

  DispatchDirectoryChangeEventImplCallback
      dispatch_directory_change_event_impl_;

  // Set this to true to ignore the DoFilesSwaWindowsExist check for testing.
  bool force_broadcasting_for_testing_ = false;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  display::ScopedDisplayObserver display_observer_{this};

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EventRouter> weak_factory_{this};
};

file_manager_private::MountError MountErrorToMountCompletedStatus(
    ash::MountError error);

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_H_
