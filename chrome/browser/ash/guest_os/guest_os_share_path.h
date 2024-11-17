// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SHARE_PATH_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SHARE_PATH_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "components/keyed_service/core/keyed_service.h"

namespace guest_os {

using SuccessCallback =
    base::OnceCallback<void(bool success, const std::string& failure_reason)>;

struct SharedPathInfo {
  explicit SharedPathInfo(std::unique_ptr<base::FilePathWatcher> watcher,
                          const std::string& vm_name);
  SharedPathInfo(SharedPathInfo&&);
  ~SharedPathInfo();

  std::unique_ptr<base::FilePathWatcher> watcher;
  std::set<std::string> vm_names;
};

// Handles sharing and unsharing paths from the Chrome OS host to guest VMs via
// seneschal.
class GuestOsSharePath : public KeyedService,
                         ash::ConciergeClient::VmObserver,
                         file_manager::VolumeManagerObserver,
                         drivefs::DriveFsHost::Observer {
 public:
  using SharePathCallback =
      base::OnceCallback<void(const base::FilePath&, bool, const std::string&)>;
  using SeneschalCallback =
      base::RepeatingCallback<void(const std::string& operation,
                                   const base::FilePath& cros_path,
                                   const base::FilePath& container_path,
                                   bool result,
                                   const std::string& failure_reason)>;
  class Observer {
   public:
    virtual void OnPersistedPathRegistered(const std::string& vm_name,
                                           const base::FilePath& path) = 0;
    virtual void OnUnshare(const std::string& vm_name,
                           const base::FilePath& path) = 0;
    virtual void OnGuestRegistered(const guest_os::GuestId& guest) = 0;
    virtual void OnGuestUnregistered(const guest_os::GuestId& guest) = 0;
  };

  // ConvertArgsToPathsToShare returns this.
  struct PathsToShare {
    PathsToShare();
    PathsToShare(PathsToShare&);
    ~PathsToShare();

    std::vector<base::FilePath> paths_to_share;
    std::vector<std::string> launch_args;
  };

  explicit GuestOsSharePath(Profile* profile);

  GuestOsSharePath(const GuestOsSharePath&) = delete;
  GuestOsSharePath& operator=(const GuestOsSharePath&) = delete;

  ~GuestOsSharePath() override;

  // KeyedService:
  // FilePathWatchers are removed in Shutdown to ensure they are all destroyed
  // before the service.
  void Shutdown() override;

  // Observer receives unshare events.
  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // Convert launch args and return paths to share with the VM, and string args
  // to pass to the app being launched. On failure, returns an error string
  // instead.
  absl::variant<PathsToShare, std::string> ConvertArgsToPathsToShare(
      const guest_os::GuestOsRegistryService::Registration& registration,
      const std::vector<guest_os::LaunchArg>& args,
      const base::FilePath& vm_mount,
      bool map_crostini_home);

  // Share specified absolute |path| with vm. If |persist| is set, the path will
  // be automatically shared at container startup. Callback receives path mapped
  // in container, success bool and failure reason string.
  void SharePath(const std::string& vm_name,
                 uint32_t seneschal_server_handle,
                 const base::FilePath& path,
                 SharePathCallback callback);

  // Share specified absolute |paths| with vm. If |persist| is set, the paths
  // will be automatically shared at container startup. Callback receives
  // success bool and failure reason string of the first error.
  void SharePaths(const std::string& vm_name,
                  uint32_t seneschal_server_handle,
                  std::vector<base::FilePath> paths,
                  SuccessCallback callback);

  // Unshare specified |path| with |vm_name|.  If |unpersist| is set, the path
  // is removed from prefs, and will not be shared at container startup.
  // Callback receives success bool and failure reason string.
  void UnsharePath(const std::string& vm_name,
                   const base::FilePath& path,
                   bool unpersist,
                   SuccessCallback callback);

  // Returns true the first time it is called on this service.
  bool GetAndSetFirstForSession(const std::string& vm_name);

  // Get list of all shared paths for the specified VM.
  std::vector<base::FilePath> GetPersistedSharedPaths(
      const std::string& vm_name);

  // Share all paths configured in prefs for the specified VM.
  // Called at container startup.  Callback is invoked once complete.
  void SharePersistedPaths(const std::string& vm_name,
                           uint32_t seneschal_server_handle,
                           SuccessCallback callback);

  // Save |paths| into prefs for |vm_name|.
  void RegisterPersistedPaths(const std::string& vm_name,
                              const std::vector<base::FilePath>& path);

  // Returns true if |path| or a parent is shared with |vm_name|.
  bool IsPathShared(const std::string& vm_name, base::FilePath path) const;

  // ash::ConciergeClient::VmObserver
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;

  // file_manager::VolumeManagerObserver
  void OnVolumeMounted(ash::MountError error_code,
                       const file_manager::Volume& volume) override;
  void OnVolumeUnmounted(ash::MountError error_code,
                         const file_manager::Volume& volume) override;

  // DriveFsHost::Observer implementation.
  void OnFilesChanged(
      const std::vector<drivefs::mojom::FileChange>& changes) override;

  // Registers |path| as shared with |vm_name|.  Adds a FilePathWatcher to
  // detect when the path has been deleted.  If the path is deleted, we unshare
  // the path, and remove it from prefs if it was persisted.
  // Visible for testing.
  void RegisterSharedPath(const std::string& vm_name,
                          const base::FilePath& path);

  // Runs on UI Thread to handle when a path is deleted.
  // Visible for testing.
  void PathDeleted(const base::FilePath& path);

  // Registers `guest` with this service, so methods which take a VmType will
  // operate on it.
  void RegisterGuest(const GuestId& guest);

  // Unregisters `guest` so it no longer is included by methods taking a
  // `VmType`.
  void UnregisterGuest(const GuestId& guest);

  // Returns the list of guests which are currently registered with this
  // service.
  const base::flat_set<GuestId>& ListGuests();

  // Allow seneschal callback to be overridden for testing.
  void set_seneschal_callback_for_testing(SeneschalCallback callback) {
    seneschal_callback_ = std::move(callback);
  }

 private:
  void CallSeneschalSharePath(const std::string& vm_name,
                              uint32_t seneschal_server_handle,
                              const base::FilePath& path,
                              SharePathCallback callback);

  void CallSeneschalUnsharePath(const std::string& vm_name,
                                const base::FilePath& path,
                                SuccessCallback callback);

  void OnFileWatcherDeleted(const base::FilePath& path);

  void OnVolumeMountCheck(const base::FilePath& path, bool mount_exists);

  // Returns info for specified path or nullptr if not found.
  SharedPathInfo* FindSharedPathInfo(const base::FilePath& path);
  // Removes |vm_name| from |info.vm_names| if it exists, and deletes the
  // |info.watcher| if |info.path| is not shared with any other VMs.  Returns
  // true if path is no longer shared with any VMs.
  bool RemoveSharedPathInfo(SharedPathInfo& info, const std::string& vm_name);

  raw_ptr<Profile> profile_;
  // Task runner for FilePathWatchers to be created, run, and be destroyed on.
  scoped_refptr<base::SequencedTaskRunner> file_watcher_task_runner_;

  // List of VMs GetAndSetFirstForSession has been called on.
  std::set<std::string> first_for_session_;

  // Allow seneschal callback to be overridden for testing.
  SeneschalCallback seneschal_callback_;
  base::ObserverList<Observer>::Unchecked observers_;
  std::map<base::FilePath, SharedPathInfo> shared_paths_;
  base::flat_set<GuestId> guests_;

  base::ScopedObservation<file_manager::VolumeManager,
                          file_manager::VolumeManagerObserver>
      volume_manager_observer_{this};

  base::WeakPtrFactory<GuestOsSharePath> weak_ptr_factory_{this};
};  // class

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SHARE_PATH_H_
