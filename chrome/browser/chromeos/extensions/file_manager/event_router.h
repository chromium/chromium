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

#include "base/compiler_specific.h"
#include "base/files/file_path_watcher.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/extensions/file_manager/device_event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/drivefs_event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/job_event_router.h"
#include "chrome/browser/chromeos/file_manager/file_watcher.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_observer.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/settings/timezone_settings.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/drive/chromeos/file_system_observer.h"
#include "components/drive/chromeos/sync_client.h"
#include "components/drive/service/drive_service_interface.h"
#include "components/keyed_service/core/keyed_service.h"
#include "storage/browser/fileapi/file_system_operation.h"

class PrefChangeRegistrar;
class Profile;

using file_manager::util::EntryDefinition;

namespace chromeos {
class NetworkState;
}

namespace drive {
class FileChange;
}

namespace file_manager {

// Monitors changes in disk mounts, network connection state and preferences
// affecting File Manager. Dispatches appropriate File Browser events.
class EventRouter : public KeyedService,
                    public chromeos::NetworkStateHandlerObserver,
                    public chromeos::system::TimezoneSettings::Observer,
                    public drive::FileSystemObserver,
                    public drive::DriveServiceObserver,
                    public VolumeManagerObserver,
                    public arc::ArcIntentHelperObserver {
 public:
  typedef base::Callback<void(const base::FilePath& virtual_path,
                              const drive::FileChange* list,
                              bool got_error,
                              const std::vector<std::string>& extension_ids)>
      DispatchDirectoryChangeEventImplCallback;

  explicit EventRouter(Profile* profile);
  ~EventRouter() override;

  // arc::ArcIntentHelperObserver overrides.
  void OnIntentFiltersUpdated() override;

  // KeyedService overrides.
  void Shutdown() override;

  typedef base::Callback<void(bool success)> BoolCallback;

  // Adds a file watch at |local_path|, associated with |virtual_path|, for
  // an extension with |extension_id|.
  //
  // |callback| will be called with true on success, or false on failure.
  // |callback| must not be null.
  //
  // Obsolete. Used as fallback for files which backends do not implement the
  // storage::WatcherManager interface.
  void AddFileWatch(const base::FilePath& local_path,
                    const base::FilePath& virtual_path,
                    const std::string& extension_id,
                    const BoolCallback& callback);

  // Removes a file watch at |local_path| for an extension with |extension_id|.
  //
  // Obsolete. Used as fallback for files which backends do not implement the
  // storage::WatcherManager interface.
  void RemoveFileWatch(const base::FilePath& local_path,
                       const std::string& extension_id);

  // Called when a copy task is completed.
  void OnCopyCompleted(
      int copy_id, const GURL& source_url, const GURL& destination_url,
      base::File::Error error);

  // Called when a copy task progress is updated.
  void OnCopyProgress(int copy_id,
                      storage::FileSystemOperation::CopyProgressType type,
                      const GURL& source_url,
                      const GURL& destination_url,
                      int64_t size);

  // Called when a notification from a watcher manager arrives.
  void OnWatcherManagerNotification(
      const storage::FileSystemURL& file_system_url,
      const std::string& extension_id,
      storage::WatcherManager::ChangeType change_type);

  // chromeos::NetworkStateHandlerObserver overrides.
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;

  // chromeos::system::TimezoneSettings::Observer overrides.
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // drive::DriveServiceObserver overrides.
  void OnRefreshTokenInvalid() override;
  void OnReadyToSendRequests() override;

  // drive::FileSystemObserver overrides.
  void OnDirectoryChanged(const base::FilePath& drive_path) override;
  void OnFileChanged(const drive::FileChange& changed_files) override;
  void OnDriveSyncError(drive::file_system::DriveSyncErrorType type,
                        const base::FilePath& drive_path) override;

  // VolumeManagerObserver overrides.
  void OnDiskAdded(const chromeos::disks::Disk& disk, bool mounting) override;
  void OnDiskRemoved(const chromeos::disks::Disk& disk) override;
  void OnDeviceAdded(const std::string& device_path) override;
  void OnDeviceRemoved(const std::string& device_path) override;
  void OnVolumeMounted(chromeos::MountError error_code,
                       const Volume& volume) override;
  void OnVolumeUnmounted(chromeos::MountError error_code,
                         const Volume& volume) override;
  void OnFormatStarted(const std::string& device_path, bool success) override;
  void OnFormatCompleted(const std::string& device_path, bool success) override;
  void OnRenameStarted(const std::string& device_path, bool success) override;
  void OnRenameCompleted(const std::string& device_path, bool success) override;
  // Set custom dispatch directory change event implementation for testing.
  void SetDispatchDirectoryChangeEventImplForTesting(
      const DispatchDirectoryChangeEventImplCallback& callback);

  // Returns a weak pointer for the event router.
  base::WeakPtr<EventRouter> GetWeakPtr();

 private:
  // Starts observing file system change events.
  void ObserveEvents();

  // Called when prefs related to file manager change.
  void OnFileManagerPrefsChanged();

  // Process file watch notifications.
  void HandleFileWatchNotification(const drive::FileChange* list,
                                   const base::FilePath& path,
                                   bool got_error);

  // Sends directory change event.
  void DispatchDirectoryChangeEvent(
      const base::FilePath& path,
      const drive::FileChange* list,
      bool got_error,
      const std::vector<std::string>& extension_ids);

  // Default implementation of DispatchDirectoryChangeEvent.
  void DispatchDirectoryChangeEventImpl(
      const base::FilePath& path,
      const drive::FileChange* list,
      bool got_error,
      const std::vector<std::string>& extension_ids);

  // Sends directory change event, after converting the file definition to entry
  // definition.
  void DispatchDirectoryChangeEventWithEntryDefinition(
      std::unique_ptr<drive::FileChange> list,
      const std::string* extension_id,
      bool watcher_error,
      const EntryDefinition& entry_definition);

  // Dispatches the mount completed event.
  void DispatchMountCompletedEvent(
      extensions::api::file_manager_private::MountCompletedEventType event_type,
      chromeos::MountError error,
      const Volume& volume);

  base::Time last_copy_progress_event_;

  std::map<base::FilePath, std::unique_ptr<FileWatcher>> file_watchers_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  Profile* profile_;

  std::unique_ptr<DeviceEventRouter> device_event_router_;
  std::unique_ptr<JobEventRouter> job_event_router_;
  std::unique_ptr<DriveFsEventRouter> drivefs_event_router_;

  DispatchDirectoryChangeEventImplCallback
      dispatch_directory_change_event_impl_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EventRouter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EventRouter);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_H_
