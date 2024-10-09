// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_share_path.h"

#include <optional>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "base/barrier_callback.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/smb_client/smb_service.h"
#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/browser/ash/smb_client/smbfs_share.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace {

// Root path under which FUSE filesystems such as DriveFS, SmbFs are mounted.
constexpr base::FilePath::CharType kFuseFsRootPath[] =
    FILE_PATH_LITERAL("/media/fuse");

void OnSeneschalSharePathResponse(
    guest_os::GuestOsSharePath::SharePathCallback callback,
    std::optional<vm_tools::seneschal::SharePathResponse> response) {
  if (!response) {
    std::move(callback).Run(base::FilePath(), false, "System error");
    return;
  }

  std::move(callback).Run(base::FilePath(response.value().path()),
                          response.value().success(),
                          response.value().failure_reason());
}

void OnSeneschalUnsharePathResponse(
    guest_os::SuccessCallback callback,
    std::optional<vm_tools::seneschal::UnsharePathResponse> response) {
  if (!response) {
    std::move(callback).Run(false, "System error");
    return;
  }
  std::move(callback).Run(response.value().success(),
                          response.value().failure_reason());
}

void LogErrorResult(const std::string& operation,
                    const base::FilePath& cros_path,
                    const base::FilePath& container_path,
                    bool result,
                    const std::string& failure_reason) {
  if (!result) {
    LOG(WARNING) << "Error " << operation << " " << cros_path << ": "
                 << failure_reason;
  }
}

struct SharePathResponseData {
  base::FilePath cros_path;
  base::FilePath container_path;
  bool success;
  std::string failure_reason;
};

SharePathResponseData AssembleSharePathResponseData(
    const base::FilePath& cros_path,
    const base::FilePath& container_path,
    bool success,
    const std::string& failure_reason) {
  return {.cros_path = cros_path,
          .container_path = container_path,
          .success = success,
          .failure_reason = failure_reason};
}

void OnGotSharePathResponses(guest_os::SuccessCallback callback,
                             std::vector<SharePathResponseData> responses) {
  for (const auto& response : responses) {
    if (!response.success) {
      LOG(WARNING) << "Error SharePath=" << response.cros_path
                   << ", FailureReason=" << response.failure_reason;
      std::move(callback).Run(/*success=*/false, response.failure_reason);
      return;
    }
  }
  std::move(callback).Run(/*success=*/true, /*failure_reason=*/"");
}

void RemovePersistedPathFromPrefs(base::Value::Dict& shared_paths,
                                  const std::string& vm_name,
                                  const base::FilePath& path) {
  // |shared_paths| format is {'path': ['vm1', vm2']}.
  // If |path| exists, remove |vm_name| from list of VMs.
  base::Value::List* found = shared_paths.FindList(path.value());
  if (!found) {
    LOG(WARNING) << "Path not in prefs to unshare path " << path.value()
                 << " for VM " << vm_name;
    return;
  }
  auto it = base::ranges::find(*found, base::Value(vm_name));
  if (it == found->end()) {
    LOG(WARNING) << "VM not in prefs to unshare path " << path.value()
                 << " for VM " << vm_name;
    return;
  }
  found->erase(it);
  // If VM list is now empty, remove |path| from |shared_paths|.
  if (found->empty()) {
    shared_paths.Remove(path.value());
  }
}

// Same as parent.AppendRelativePath(child, path) except that it allows
// parent == child, in which case path is unchanged.
bool AppendRelativePath(const base::FilePath& parent,
                        const base::FilePath& child,
                        base::FilePath* path) {
  return child == parent || parent.AppendRelativePath(child, path);
}

}  // namespace

namespace guest_os {

SharedPathInfo::SharedPathInfo(std::unique_ptr<base::FilePathWatcher> watcher,
                               const std::string& vm_name)
    : watcher(std::move(watcher)) {
  vm_names.insert(vm_name);
}
SharedPathInfo::SharedPathInfo(SharedPathInfo&&) = default;
SharedPathInfo::~SharedPathInfo() = default;

GuestOsSharePath::PathsToShare::PathsToShare() = default;
GuestOsSharePath::PathsToShare::PathsToShare(GuestOsSharePath::PathsToShare&) =
    default;
GuestOsSharePath::PathsToShare::~PathsToShare() = default;

GuestOsSharePath::GuestOsSharePath(Profile* profile)
    : profile_(profile),
      file_watcher_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      seneschal_callback_(base::BindRepeating(LogErrorResult)) {
  if (auto* client = ash::ConciergeClient::Get()) {
    client->AddVmObserver(this);
  }

  if (auto* vmgr = file_manager::VolumeManager::Get(profile_)) {
    volume_manager_observer_.Observe(vmgr);
  }

  // We receive notifications from DriveFS about any deleted paths so
  // that we can remove any that are shared paths.
  if (drive::DriveIntegrationService* const service =
          drive::DriveIntegrationServiceFactory::FindForProfile(profile_)) {
    Observe(service->GetDriveFsHost());
  }
}

GuestOsSharePath::~GuestOsSharePath() = default;

void GuestOsSharePath::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* client = ash::ConciergeClient::Get()) {
    client->RemoveVmObserver(this);
  }

  for (auto& shared_path : shared_paths_) {
    if (shared_path.second.watcher) {
      file_watcher_task_runner_->DeleteSoon(
          FROM_HERE, shared_path.second.watcher.release());
    }
  }
}

void GuestOsSharePath::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void GuestOsSharePath::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void GuestOsSharePath::CallSeneschalSharePath(const std::string& vm_name,
                                              uint32_t seneschal_server_handle,
                                              const base::FilePath& path,
                                              SharePathCallback callback) {
  // Verify path is in one of the allowable mount points.
  // This logic is similar to DownloadPrefs::SanitizeDownloadTargetPath().
  if (!path.IsAbsolute() || path.ReferencesParent()) {
    std::move(callback).Run(base::FilePath(), false, "Path must be absolute");
    return;
  }

  vm_tools::seneschal::SharePathRequest request;
  base::FilePath fuse_fs_root_path(kFuseFsRootPath);
  base::FilePath drivefs_path;
  base::FilePath relative_path;
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  base::FilePath drivefs_mount_point_path;
  base::FilePath drivefs_mount_name;
  auto* smb_service = ash::smb_client::SmbServiceFactory::Get(profile_);
  ash::smb_client::SmbFsShare* smb_share = nullptr;
  base::FilePath smbfs_mount_point_path;
  base::FilePath smbfs_mount_name;
  auto* vmgr = file_manager::VolumeManager::Get(profile_);
  base::WeakPtr<file_manager::Volume> volume;

  // Allow MyFiles directory and subdirs.
  bool allowed_path = false;
  base::FilePath my_files =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  base::FilePath android_files(file_manager::util::GetAndroidFilesPath());
  base::FilePath removable_media(file_manager::util::kRemovableMediaPath);
  base::FilePath linux_files =
      file_manager::util::GetCrostiniMountDirectory(profile_);
  base::FilePath system_fonts(file_manager::util::kSystemFontsPath);
  base::FilePath archive_mount(file_manager::util::kArchiveMountPath);
  base::FilePath fusebox_path(file_manager::util::kFuseBoxMediaPath);
  if (AppendRelativePath(my_files, path, &relative_path)) {
    allowed_path = true;
    request.set_storage_location(
        vm_tools::seneschal::SharePathRequest::MY_FILES);
    request.set_owner_id(crostini::CryptohomeIdForProfile(profile_));
  } else if (integration_service &&
             (drivefs_mount_point_path =
                  integration_service->GetMountPointPath())
                 .AppendRelativePath(path, &drivefs_path) &&
             fuse_fs_root_path.AppendRelativePath(drivefs_mount_point_path,
                                                  &drivefs_mount_name)) {
    // Allow subdirs of DriveFS (/media/fuse/drivefs-*) except .Trash-1000.
    request.set_drivefs_mount_name(drivefs_mount_name.value());
    base::FilePath root("root");
    base::FilePath team_drives("team_drives");
    base::FilePath computers("Computers");
    base::FilePath files_by_id(".files-by-id");
    base::FilePath shortcut_targets_by_id(".shortcut-targets-by-id");
    base::FilePath trash(".Trash-1000");  // Not to be shared!
    if (AppendRelativePath(root, drivefs_path, &relative_path)) {
      // My Drive and subdirs.
      allowed_path = true;
      request.set_storage_location(
          vm_tools::seneschal::SharePathRequest::DRIVEFS_MY_DRIVE);
    } else if (AppendRelativePath(team_drives, drivefs_path, &relative_path)) {
      // Team Drives and subdirs.
      allowed_path = true;
      request.set_storage_location(
          vm_tools::seneschal::SharePathRequest::DRIVEFS_TEAM_DRIVES);
    } else if (AppendRelativePath(computers, drivefs_path, &relative_path)) {
      // Computers and subdirs.
      allowed_path = true;
      request.set_storage_location(
          vm_tools::seneschal::SharePathRequest::DRIVEFS_COMPUTERS);

      // TODO(crbug.com/40607763): Do not allow Computers Grand Root, or single
      // Computer Root to be shared until DriveFS enforces allowed write paths.
      std::vector<base::FilePath::StringType> components =
          relative_path.GetComponents();
      if (components.size() < 2) {
        allowed_path = false;
      }
    } else if (AppendRelativePath(files_by_id, drivefs_path, &relative_path)) {
      // Shared (.files-by-id) and subdirs.
      allowed_path = true;
      request.set_storage_location(
          vm_tools::seneschal::SharePathRequest::DRIVEFS_FILES_BY_ID);
    } else if (AppendRelativePath(shortcut_targets_by_id, drivefs_path,
                                  &relative_path)) {
      // Shared (.shortcut-targets-by-id) and subdirs.
      allowed_path = true;
      request.set_storage_location(vm_tools::seneschal::SharePathRequest::
                                       DRIVEFS_SHORTCUT_TARGETS_BY_ID);
    } else if (trash == drivefs_path || trash.IsParent(drivefs_path)) {
      // Note: Do not expose .Trash-1000 which would allow linux apps to make
      // permanent deletes from Drive.  This branch is not especially required,
      // but is included to make it explicit that .Trash-1000 should not be
      // shared.
      allowed_path = false;
    }
  } else if (AppendRelativePath(android_files, path, &relative_path)) {
    // Allow Android files and subdirs.
    allowed_path = true;
    request.set_storage_location(
        arc::IsArcVmEnabled()
            ? vm_tools::seneschal::SharePathRequest::PLAY_FILES_GUEST_OS
            : vm_tools::seneschal::SharePathRequest::PLAY_FILES);
  } else if (removable_media.AppendRelativePath(path, &relative_path)) {
    // Allow subdirs of /media/removable.
    allowed_path = true;
    request.set_storage_location(
        vm_tools::seneschal::SharePathRequest::REMOVABLE);
  } else if (AppendRelativePath(linux_files, path, &relative_path)) {
    // Allow Linux files and subdirs.
    allowed_path = true;
    request.set_storage_location(
        vm_tools::seneschal::SharePathRequest::LINUX_FILES);
    request.set_owner_id(crostini::CryptohomeIdForProfile(profile_));
  } else if (AppendRelativePath(system_fonts, path, &relative_path)) {
    // Allow /usr/share/fonts and subdirs.
    allowed_path = true;
    request.set_storage_location(vm_tools::seneschal::SharePathRequest::FONTS);
  } else if (archive_mount.AppendRelativePath(path, &relative_path)) {
    // Allow subdirs of /media/archive.
    allowed_path = true;
    request.set_storage_location(
        vm_tools::seneschal::SharePathRequest::ARCHIVE);
  } else if (fusebox_path.AppendRelativePath(path, &relative_path)) {
    // Allow Fusebox files and subdirs under /media/fuse/fusebox.
    allowed_path = true;
    request.set_storage_location(
        vm_tools::seneschal::SharePathRequest::FUSEBOX);
  } else if (smb_service &&
             (smb_share = smb_service->GetSmbFsShareForPath(path)) &&
             AppendRelativePath(
                 smbfs_mount_point_path = smb_share->mount_path(), path,
                 &relative_path) &&
             fuse_fs_root_path.AppendRelativePath(smbfs_mount_point_path,
                                                  &smbfs_mount_name)) {
    // Allow smbfs mounts (/media/fuse/smbfs-*) and subdirs.
    allowed_path = true;
    request.set_storage_location(vm_tools::seneschal::SharePathRequest::SMBFS);
    request.set_smbfs_mount_name(smbfs_mount_name.value());
  } else if (vmgr && (volume = vmgr->FindVolumeFromPath(path)) &&
             volume->type() == file_manager::VOLUME_TYPE_GUEST_OS &&
             AppendRelativePath(volume->mount_path(), path, &relative_path)) {
    allowed_path = true;
    // Allow GuestOs files and subdirs.
    base::FilePath mount_name;
    fuse_fs_root_path.AppendRelativePath(volume->mount_path(), &mount_name);
    request.set_storage_location(
        vm_tools::seneschal::SharePathRequest::GUEST_OS_FILES);
    request.set_owner_id(crostini::CryptohomeIdForProfile(profile_));
    request.set_guest_os_mount_name(mount_name.value());
  }

  if (!allowed_path) {
    std::move(callback).Run(base::FilePath(), false, "Path is not allowed");
    return;
  }

  // We will not make a blocking call to verify the path exists since
  // we don't want to block, and seneschal must verify this regardless.
  RegisterSharedPath(vm_name, path);

  request.mutable_shared_path()->set_path(relative_path.value());
  request.mutable_shared_path()->set_writable(true);
  request.set_handle(seneschal_server_handle);

  ash::SeneschalClient::Get()->SharePath(
      request,
      base::BindOnce(&OnSeneschalSharePathResponse, std::move(callback)));
}

void GuestOsSharePath::CallSeneschalUnsharePath(const std::string& vm_name,
                                                const base::FilePath& path,
                                                SuccessCallback callback) {
  vm_tools::seneschal::UnsharePathRequest request;

  // Return success if VM is not currently running.
  auto vm_info =
      GuestOsSessionTrackerFactory::GetForProfile(profile_)->GetVmInfo(vm_name);
  if (!vm_info) {
    std::move(callback).Run(true, "VM not running");
    return;
  }
  request.set_handle(vm_info->seneschal_server_handle());

  // Convert path to a virtual path relative to one of the external mounts,
  // then get it as a FilesSystemURL to convert to a path inside the VM,
  // then remove mount base dir prefix to get the path to unshare.
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath virtual_path;
  base::FilePath dummy_vm_mount("/");
  base::FilePath inside;
  bool result = mount_points->GetVirtualPath(path, &virtual_path);
  if (result) {
    storage::FileSystemURL url = mount_points->CreateCrackedFileSystemURL(
        blink::StorageKey(), storage::kFileSystemTypeExternal, virtual_path);
    result = file_manager::util::ConvertFileSystemURLToPathInsideVM(
        profile_, url, dummy_vm_mount, /*map_crostini_home=*/false, &inside);
  } else {
    // Fusebox Monikers do not belong to any external mounts, so their paths are
    // directly translated to the ones inside VMs.
    result = file_manager::util::ConvertFuseboxMonikerPathToPathInsideVM(
        path, dummy_vm_mount, &inside);
  }
  base::FilePath unshare_path;
  if (!result || !dummy_vm_mount.AppendRelativePath(inside, &unshare_path)) {
    std::move(callback).Run(false, "Invalid path to unshare");
    return;
  }

  request.set_path(unshare_path.value());
  ash::SeneschalClient::Get()->UnsharePath(
      request,
      base::BindOnce(&OnSeneschalUnsharePathResponse, std::move(callback)));
}

void GuestOsSharePath::SharePath(const std::string& vm_name,
                                 uint32_t seneschal_server_handle,
                                 const base::FilePath& path,
                                 SharePathCallback callback) {
  DCHECK(callback);
  CallSeneschalSharePath(vm_name, seneschal_server_handle, path,
                         std::move(callback));
}

void GuestOsSharePath::SharePaths(const std::string& vm_name,
                                  uint32_t seneschal_server_handle,
                                  std::vector<base::FilePath> paths,
                                  SuccessCallback callback) {
  if (paths.empty()) {
    std::move(callback).Run(true, "");
    return;
  }
  auto barrier = base::BarrierCallback<SharePathResponseData>(
      paths.size(),
      base::BindOnce(&OnGotSharePathResponses, std::move(callback)));
  for (const auto& path : paths) {
    CallSeneschalSharePath(
        vm_name, seneschal_server_handle, path,
        base::BindOnce(&AssembleSharePathResponseData, path).Then(barrier));
  }
}

void GuestOsSharePath::UnsharePath(const std::string& vm_name,
                                   const base::FilePath& path,
                                   bool unpersist,
                                   SuccessCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* info = FindSharedPathInfo(path)) {
    if (RemoveSharedPathInfo(*info, vm_name)) {
      shared_paths_.erase(path);
    }
  }

  if (unpersist) {
    PrefService* pref_service = profile_->GetPrefs();
    ScopedDictPrefUpdate update(pref_service, prefs::kGuestOSPathsSharedToVms);
    RemovePersistedPathFromPrefs(*update, vm_name, path);
  }

  CallSeneschalUnsharePath(vm_name, path, std::move(callback));
  for (Observer& observer : observers_) {
    observer.OnUnshare(vm_name, path);
  }
}

bool GuestOsSharePath::GetAndSetFirstForSession(const std::string& vm_name) {
  auto result = first_for_session_.insert(vm_name);
  return result.second;
}

std::vector<base::FilePath> GuestOsSharePath::GetPersistedSharedPaths(
    const std::string& vm_name) {
  std::vector<base::FilePath> result;
  // TODO(crbug.com/40677501): Unexpected crashes here.
  CHECK(profile_);
  CHECK(profile_->GetPrefs());
  // |shared_paths| format is {'path': ['vm1', vm2']}.
  const base::Value::Dict& shared_paths =
      profile_->GetPrefs()->GetDict(prefs::kGuestOSPathsSharedToVms);
  for (const auto it : shared_paths) {
    base::FilePath path(it.first);
    for (const auto& vm : it.second.GetList()) {
      // Register all shared paths for all VMs since we want FilePathWatchers
      // to start immediately.
      RegisterSharedPath(vm.GetString(), path);
      // Only add to result if path is shared with specified |vm_name|.
      if (vm.GetString() == vm_name) {
        result.emplace_back(path);
      }
    }
  }
  return result;
}

void GuestOsSharePath::SharePersistedPaths(const std::string& vm_name,
                                           uint32_t seneschal_server_handle,
                                           SuccessCallback callback) {
  SharePaths(vm_name, seneschal_server_handle, GetPersistedSharedPaths(vm_name),
             std::move(callback));
}

void GuestOsSharePath::RegisterPersistedPaths(
    const std::string& vm_name,
    const std::vector<base::FilePath>& paths) {
  PrefService* pref_service = profile_->GetPrefs();
  ScopedDictPrefUpdate update(pref_service, prefs::kGuestOSPathsSharedToVms);
  base::Value::Dict& shared_paths = *update;
  for (const auto& path : paths) {
    // Check if path is already shared so we know whether we need to add it.
    bool already_shared = false;
    // Remove any paths that are children of this path.
    // E.g. if path /foo/bar is already shared, and then we share /foo, we
    // remove /foo/bar from the list since it will be shared as part of /foo.
    std::vector<base::FilePath> children;
    for (const auto it : shared_paths) {
      base::FilePath shared(it.first);
      auto& vms = it.second;
      auto vm_matches = base::Contains(vms.GetList(), base::Value(vm_name));
      if (path == shared) {
        already_shared = true;
        if (!vm_matches) {
          vms.GetList().Append(vm_name);
        }
      } else if (path.IsParent(shared) && vm_matches) {
        children.emplace_back(shared);
      }
    }
    for (const auto& child : children) {
      RemovePersistedPathFromPrefs(shared_paths, vm_name, child);
    }
    if (!already_shared) {
      base::Value::List vms;
      vms.Append(vm_name);
      shared_paths.Set(path.value(), std::move(vms));
    }
    for (Observer& observer : observers_) {
      observer.OnPersistedPathRegistered(vm_name, path);
    }
  }
}

bool GuestOsSharePath::IsPathShared(const std::string& vm_name,
                                    base::FilePath path) const {
  while (true) {
    auto it = shared_paths_.find(path);
    if (it != shared_paths_.end() && it->second.vm_names.count(vm_name) > 0) {
      return true;
    }
    base::FilePath parent = path.DirName();
    if (parent == path) {
      return false;
    }
    path = std::move(parent);
  }
}

void GuestOsSharePath::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {
  // SharePersistedPaths fetches the seneschal handle from other services which
  // also observe OnVmStarted. So we `PostTask` instead of running
  // synchronously to give them a chance to update first.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GuestOsSharePath::SharePersistedPaths,
          weak_ptr_factory_.GetWeakPtr(), signal.name(),
          signal.vm_info().seneschal_server_handle(),
          base::BindOnce([](bool success, const std::string& failure_reason) {
            if (!success) {
              LOG(ERROR) << "Error sharing persistent paths: "
                         << failure_reason;
            }
          })));
}

void GuestOsSharePath::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto it = shared_paths_.begin(); it != shared_paths_.end();) {
    if (RemoveSharedPathInfo(it->second, signal.name())) {
      shared_paths_.erase(it++);
    } else {
      ++it;
    }
  }
}

void GuestOsSharePath::OnVolumeMounted(ash::MountError error_code,
                                       const file_manager::Volume& volume) {
  if (error_code != ash::MountError::kSuccess) {
    return;
  }

  // Check if any persisted paths match volume.mount_path() or are children
  // of it then share them with any running VMs.
  const base::Value::Dict& shared_paths =
      profile_->GetPrefs()->GetDict(prefs::kGuestOSPathsSharedToVms);
  for (const auto it : shared_paths) {
    base::FilePath path(it.first);
    if (path != volume.mount_path() && !volume.mount_path().IsParent(path)) {
      continue;
    }
    const auto& vms = it.second.GetList();
    for (const auto& vm : vms) {
      RegisterSharedPath(vm.GetString(), path);
      auto vm_info =
          GuestOsSessionTrackerFactory::GetForProfile(profile_)->GetVmInfo(
              vm.GetString());
      if (vm_info) {
        CallSeneschalSharePath(
            vm.GetString(), vm_info->seneschal_server_handle(), path,
            base::BindOnce(seneschal_callback_, "share-on-mount", path));
      }
    }
  }
}

void GuestOsSharePath::OnVolumeUnmounted(ash::MountError error_code,
                                         const file_manager::Volume& volume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error_code != ash::MountError::kSuccess) {
    return;
  }
  for (auto it = shared_paths_.begin(); it != shared_paths_.end();) {
    // Defensive copy of path since unsharing modifies shared_paths_.
    base::FilePath path(it->first);
    if (path == volume.mount_path() || volume.mount_path().IsParent(path)) {
      // Defensive copy of vm_names for same reason.
      const std::set<std::string> vm_names(it->second.vm_names);
      ++it;
      for (auto& vm_name : vm_names) {
        // Unshare with unpersist=false since we still want the path
        // to be persisted when volume is next mounted.
        UnsharePath(vm_name, path, /*unpersist=*/false,
                    base::BindOnce(seneschal_callback_, "unshare-on-unmount",
                                   path, path));
      }
    } else {
      ++it;
    }
  }
}

void GuestOsSharePath::RegisterSharedPath(const std::string& vm_name,
                                          const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Paths may be called to be shared multiple times for the same or different
  // vm.  If path is already registered, add vm_name to list of VMs shared with
  // and return.
  if (auto* info = FindSharedPathInfo(path)) {
    info->vm_names.insert(vm_name);
    return;
  }

  // |changed| is invoked by FilePathWatcher and runs on its task runner.
  // It runs |deleted| (OnFileWatcherDeleted) on the UI thread.
  auto deleted = base::BindRepeating(&GuestOsSharePath::OnFileWatcherDeleted,
                                     weak_ptr_factory_.GetWeakPtr(), path);
  auto changed = [](base::RepeatingClosure deleted, const base::FilePath& path,
                    bool error) {
    if (!error && !base::PathExists(path)) {
      content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, deleted);
    }
  };
  // Start watcher on its sequenced task runner.  It must also be destroyed
  // on the same sequence.  SequencedTaskRunner guarantees that this call will
  // complete before any calls to DeleteSoon for this object are run.
  auto watcher = std::make_unique<base::FilePathWatcher>();
  file_watcher_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&base::FilePathWatcher::Watch),
          base::Unretained(watcher.get()), path,
          base::FilePathWatcher::Type::kNonRecursive,
          base::BindRepeating(std::move(changed), std::move(deleted))));
  shared_paths_.emplace(path, SharedPathInfo(std::move(watcher), vm_name));
}

void GuestOsSharePath::OnFileWatcherDeleted(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Check if volume is still mounted.
  auto* vmgr = file_manager::VolumeManager::Get(profile_);
  if (!vmgr) {
    return;
  }
  const auto volume_list = vmgr->GetVolumeList();
  for (const auto& volume : volume_list) {
    if ((path == volume->mount_path() || volume->mount_path().IsParent(path))) {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(&base::PathExists, volume->mount_path()),
          base::BindOnce(&GuestOsSharePath::OnVolumeMountCheck,
                         weak_ptr_factory_.GetWeakPtr(), path));
      return;
    }
  }
}

void GuestOsSharePath::OnVolumeMountCheck(const base::FilePath& path,
                                          bool mount_exists) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If the Volume mount does not exist, then we assume that the path was
  // not deleted, but the volume was unmounted.  We call seneschal_callback_
  // for our tests, but otherwise do nothing and assume an UnmountEvent is
  // coming.
  if (!mount_exists) {
    seneschal_callback_.Run("ignore-delete-before-unmount", path, path, true,
                            "");
  } else {
    PathDeleted(path);
  }
}

void GuestOsSharePath::PathDeleted(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* info = FindSharedPathInfo(path);
  if (!info) {
    return;
  }

  // Defensive copy of vm_names since unsharing modifies shared_paths_.
  const std::set<std::string> vm_names(info->vm_names);
  for (auto& vm_name : vm_names) {
    UnsharePath(
        vm_name, path, /*unpersist=*/true,
        base::BindOnce(seneschal_callback_, "unshare-on-delete", path, path));
  }
}

void GuestOsSharePath::OnFilesChanged(
    const std::vector<drivefs::mojom::FileChange>& changes) {
  auto* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (!integration_service) {
    return;
  }
  // Paths come as absolute from the drivefs mount point.  E.g. /root/folder.
  base::FilePath root("/");
  for (const auto& change : changes) {
    base::FilePath path = integration_service->GetMountPointPath();
    if (change.type == drivefs::mojom::FileChange_Type::kDelete &&
        root.AppendRelativePath(change.path, &path)) {
      PathDeleted(path);
    }
  }
}

SharedPathInfo* GuestOsSharePath::FindSharedPathInfo(
    const base::FilePath& path) {
  auto it = shared_paths_.find(path);
  if (it == shared_paths_.end()) {
    return nullptr;
  }
  return &it->second;
}

bool GuestOsSharePath::RemoveSharedPathInfo(SharedPathInfo& info,
                                            const std::string& vm_name) {
  info.vm_names.erase(vm_name);
  if (info.vm_names.empty()) {
    if (info.watcher) {
      file_watcher_task_runner_->DeleteSoon(FROM_HERE, info.watcher.release());
    }
    return true;
  }
  return false;
}

void GuestOsSharePath::RegisterGuest(const GuestId& guest) {
  guests_.insert(guest);
  for (auto& observer : observers_) {
    observer.OnGuestRegistered(guest);
  }
}
void GuestOsSharePath::UnregisterGuest(const GuestId& guest) {
  guests_.erase(guest);
  for (auto& observer : observers_) {
    observer.OnGuestUnregistered(guest);
  }
}

const base::flat_set<GuestId>& GuestOsSharePath::ListGuests() {
  return guests_;
}

absl::variant<GuestOsSharePath::PathsToShare, std::string>
GuestOsSharePath::ConvertArgsToPathsToShare(
    const guest_os::GuestOsRegistryService::Registration& registration,
    const std::vector<guest_os::LaunchArg>& args,
    const base::FilePath& vm_mount,
    bool map_crostini_home) {
  PathsToShare out;
  const std::string& vm_name = registration.VmName();

  // Convert any paths not in the VM.
  out.launch_args.reserve(args.size());
  for (const auto& arg : args) {
    if (absl::holds_alternative<std::string>(arg)) {
      out.launch_args.push_back(absl::get<std::string>(arg));
      continue;
    }
    const storage::FileSystemURL& url = absl::get<storage::FileSystemURL>(arg);
    base::FilePath path;
    if (!file_manager::util::ConvertFileSystemURLToPathInsideVM(
            profile_, url, vm_mount, map_crostini_home, &path)) {
      return "Cannot share URL with VM.";
    }
    if (url.mount_filesystem_id() !=
            file_manager::util::GetGuestOsMountPointName(
                profile_, registration.ToGuestId()) &&

        !IsPathShared(vm_name, url.path())) {
      out.paths_to_share.push_back(url.path());
    }
    out.launch_args.push_back(path.value());
  }
  return out;
}

}  // namespace guest_os
