// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/disks/disk_mount_manager.h"
#include "ash/components/settings/timezone_settings.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "chrome/browser/ash/file_manager/file_watcher.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/device_event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/drivefs_event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/system_notification_manager.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

class PrefChangeRegistrar;
class Profile;

using file_manager::util::EntryDefinition;

namespace file_manager {

// Monitors changes in disk mounts, network connection state and preferences
// affecting File Manager. Dispatches appropriate File Browser events.
class EventRouter
    : public KeyedService,
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public extensions::ExtensionRegistryObserver,
      public ash::system::TimezoneSettings::Observer,
      public VolumeManagerObserver,
      public arc::ArcIntentHelperObserver,
      public drive::DriveIntegrationServiceObserver,
      public guest_os::GuestOsSharePath::Observer,
      public ash::TabletModeObserver,
      public file_manager::io_task::IOTaskController::Observer,
      public guest_os::GuestOsMountProviderRegistry::Observer {
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
      const absl::optional<std::string>& package_name) override;

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

  // Called when a copy task is started.
  void OnCopyStarted(int copy_id,
                     const GURL& source_url,
                     const GURL& destination_url,
                     int64_t space_needed);

  // Called when a copy task is completed.
  void OnCopyCompleted(
      int copy_id, const GURL& source_url, const GURL& destination_url,
      base::File::Error error);

  // Called when a copy task progress is updated.
  void OnCopyProgress(int copy_id,
                      FileManagerCopyOrMoveHookDelegate::ProgressType type,
                      const GURL& source_url,
                      const GURL& destination_url,
                      int64_t size);

  // Called when a notification from a watcher manager arrives.
  void OnWatcherManagerNotification(
      const storage::FileSystemURL& file_system_url,
      const url::Origin& listener_origin,
      storage::WatcherManager::ChangeType change_type);

  // network::NetworkConnectionTracker::NetworkConnectionObserver overrides.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

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
  void OnVolumeMounted(chromeos::MountError error_code,
                       const Volume& volume) override;
  void OnVolumeUnmounted(chromeos::MountError error_code,
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

  // DriveIntegrationServiceObserver override.
  void OnFileSystemMountFailed() override;

  // guest_os::GuestOsSharePath::Observer overrides.
  void OnShare(const std::string& vm_name,
               const base::FilePath& path,
               bool persist) override;
  void OnUnshare(const std::string& vm_name,
                 const base::FilePath& path) override;

  // ash:TabletModeObserver overrides.
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

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

 private:
  FRIEND_TEST_ALL_PREFIXES(EventRouterTest, PopulateCrostiniEvent);

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
      chromeos::MountError error,
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

  // Called for Crostini events when the specified pref value changes.
  void OnCrostiniChanged(
      const std::string& vm_name,
      const std::string& pref_name,
      extensions::api::file_manager_private::CrostiniEventType pref_true,
      extensions::api::file_manager_private::CrostiniEventType pref_false);

  // Called when Plugin VM enabled state may have changed.
  void OnPluginVmChanged();

  void NotifyDriveConnectionStatusChanged();

  void DisplayDriveConfirmDialog(
      const drivefs::mojom::DialogReason& reason,
      base::OnceCallback<void(drivefs::mojom::DialogResult)> callback);

  // Called to refresh the list of guests and broadcast it.
  void OnMountableGuestsChanged();

  base::Time last_copy_progress_event_;

  std::map<base::FilePath, std::unique_ptr<FileWatcher>> file_watchers_;
  std::unique_ptr<plugin_vm::PluginVmPolicySubscription>
      plugin_vm_subscription_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  Profile* profile_;

  std::unique_ptr<SystemNotificationManager> notification_manager_;
  std::unique_ptr<DeviceEventRouter> device_event_router_;
  std::unique_ptr<DriveFsEventRouter> drivefs_event_router_;

  DispatchDirectoryChangeEventImplCallback
      dispatch_directory_change_event_impl_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EventRouter> weak_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_H_
