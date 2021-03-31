// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_share_path.h"

#include "base/atomic_ref_count.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/smb_client/smb_service.h"
#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"
#include "chrome/browser/chromeos/smb_client/smbfs_share.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/seneschal_client.h"
#include "components/arc/arc_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace {

// Root path under which FUSE filesystems such as DriveFS, SmbFs are mounted.
constexpr base::FilePath::CharType kFuseFsRootPath[] =
    FILE_PATH_LITERAL("/media/fuse");

void OnSeneschalSharePathResponse(
    guest_os::GuestOsSharePath::SharePathCallback callback,
    base::Optional<vm_tools::seneschal::SharePathResponse> response) {
  if (!response) {
    std::move(callback).Run(base::FilePath(), false, "System error");
    return;
  }

  std::move(callback).Run(base::FilePath(response.value().path()),
                          response.value().success(),
                          response.value().failure_reason());
}

void OnVmRestartedForSeneschal(
    Profile* profile,
    const std::string& vm_name,
    guest_os::GuestOsSharePath::SharePathCallback callback,
    vm_tools::seneschal::SharePathRequest request,
    crostini::CrostiniResult result) {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile);
  base::Optional<crostini::VmInfo> vm_info =
      crostini_manager->GetVmInfo(vm_name);
  if (!vm_info || vm_info->state != crostini::VmState::STARTED) {
    std::move(callback).Run(base::FilePath(), false, "VM could not be started");
    return;
  }
  request.set_handle(vm_info->info.seneschal_server_handle());
  chromeos::DBusThreadManager::Get()->GetSeneschalClient()->SharePath(
      request,
      base::BindOnce(&OnSeneschalSharePathResponse, std::move(callback)));
}

void OnSeneschalUnsharePathResponse(
    guest_os::SuccessCallback callback,
    base::Optional<vm_tools::seneschal::UnsharePathResponse> response) {
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

// Barrier Closure that captures the first instance of error.
class ErrorCapture {
 public:
  ErrorCapture(int num_callbacks_left, guest_os::SuccessCallback callback)
      : num_callbacks_left_(num_callbacks_left),
        callback_(std::move(callback)) {
    DCHECK_GE(num_callbacks_left, 0);
    if (num_callbacks_left == 0)
      std::move(callback_).Run(true, "");
  }

  void Run(const base::FilePath& cros_path,
           const base::FilePath& container_path,
           bool success,
           const std::string& failure_reason) {
    if (!success) {
      LOG(WARNING) << "Error SharePath=" << cros_path.value()
                   << ", FailureReason=" << failure_reason;
      if (success_) {
        success_ = false;
        first_failure_reason_ = failure_reason;
      }
    }

    if (!--num_callbacks_left_)
      std::move(callback_).Run(success_, first_failure_reason_);
  }

 private:
  int num_callbacks_left_;
  guest_os::SuccessCallback callback_;
  bool success_ = true;
  std::string first_failure_reason_;
};  // class

void RemovePersistedPathFromPrefs(base::DictionaryValue* shared_paths,
                                  const std::string& vm_name,
                                  const base::FilePath& path) {
  // |shared_paths| format is {'path': ['vm1', vm2']}.
  // If |path| exists, remove |vm_name| from list of VMs.
  base::Value* found = shared_paths->FindKey(path.value());
  if (!found) {
    LOG(WARNING) << "Path not in prefs to unshare path " << path.value()
                 << " for VM " << vm_name;
    return;
  }
  auto it = std::find(found->GetList().begin(), found->GetList().end(),
                      base::Value(vm_name));
  if (!found->EraseListIter(it)) {
    LOG(WARNING) << "VM not in prefs to unshare path " << path.value()
                 << " for VM " << vm_name;
    return;
  }
  // If VM list is now empty, remove |path| from |shared_paths|.
  if (found->GetList().empty()) {
    shared_paths->RemoveKey(path.value());
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

GuestOsSharePath* GuestOsSharePath::GetForProfile(Profile* profile) {
  return GuestOsSharePathFactory::GetForProfile(profile);
}

GuestOsSharePath::GuestOsSharePath(Profile* profile)
    : profile_(profile),
      file_watcher_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      seneschal_callback_(base::BindRepeating(LogErrorResult)) {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  crostini_manager->AddVmShutdownObserver(this);

  if (auto* vmgr = file_manager::VolumeManager::Get(profile_)) {
    vmgr->AddObserver(this);
  }

  // We receive notifications from DriveFS about any deleted paths so
  // that we can remove any that are shared paths.
  if (auto* integration_service =
          drive::DriveIntegrationServiceFactory::FindForProfile(profile_)) {
    if (integration_service->GetDriveFsHost()) {
      integration_service->GetDriveFsHost()->AddObserver(this);
    }
  }
}

GuestOsSharePath::~GuestOsSharePath() = default;

void GuestOsSharePath::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  crostini_manager->RemoveVmShutdownObserver(this);

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

void GuestOsSharePath::CallSeneschalSharePath(const std::string& vm_name,
                                              const base::FilePath& path,
                                              bool persist,
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
  chromeos::smb_client::SmbService* smb_service =
      chromeos::smb_client::SmbServiceFactory::Get(profile_);
  chromeos::smb_client::SmbFsShare* smb_share = nullptr;
  base::FilePath smbfs_mount_point_path;
  base::FilePath smbfs_mount_name;

  // Allow MyFiles directory and subdirs.
  bool allowed_path = false;
  base::FilePath my_files =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  base::FilePath android_files(file_manager::util::kAndroidFilesPath);
  base::FilePath removable_media(file_manager::util::kRemovableMediaPath);
  base::FilePath linux_files =
      file_manager::util::GetCrostiniMountDirectory(profile_);
  base::FilePath system_fonts(file_manager::util::kSystemFontsPath);
  base::FilePath archive_mount(file_manager::util::kArchiveMountPath);
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
    // Allow subdirs of DriveFS (/media/fuse/drivefs-*) except .Trash.
    request.set_drivefs_mount_name(drivefs_mount_name.value());
    base::FilePath root("root");
    base::FilePath team_drives("team_drives");
    base::FilePath computers("Computers");
    base::FilePath trash(".Trash");  // Not to be shared!
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

      // TODO(crbug.com/917920): Do not allow Computers Grand Root, or single
      // Computer Root to be shared until DriveFS enforces allowed write paths.
      std::vector<base::FilePath::StringType> components;
      relative_path.GetComponents(&components);
      if (components.size() < 2) {
        allowed_path = false;
      }
    } else if (trash == drivefs_path || trash.IsParent(drivefs_path)) {
      // Note: Do not expose .Trash which would allow linux apps to make
      // permanent deletes from Drive.  This branch is not especially required,
      // but is included to make it explicit that .Trash should not be shared.
      allowed_path = false;
    }
  } else if (AppendRelativePath(android_files, path, &relative_path)) {
    // Allow Android files and subdirs.
    allowed_path = true;
    request.set_storage_location(
        vm_tools::seneschal::SharePathRequest::PLAY_FILES);
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
  }

  if (!allowed_path) {
    std::move(callback).Run(base::FilePath(), false, "Path is not allowed");
    return;
  }

  // We will not make a blocking call to verify the path exists since
  // we don't want to block, and seneschal must verify this regardless.

  // Even if VM is not running, save to prefs now.
  if (persist) {
    RegisterPersistedPath(vm_name, path);
  }
  RegisterSharedPath(vm_name, path);

  request.mutable_shared_path()->set_path(relative_path.value());
  request.mutable_shared_path()->set_writable(true);

  // Handle PluginVm.  TODO(joelhockey): If we ever require to (re)start
  // PluginVm before sharing, we can detect that the VM is not started
  // if handle == 0.
  if (vm_name == plugin_vm::kPluginVmName) {
    request.set_handle(
        plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)
            ->seneschal_server_handle());
  } else if (vm_name == arc::kArcVmName) {
    const auto& vm_info = arc::ArcSessionManager::Get()->GetVmInfo();
    if (!vm_info) {
      LOG(WARNING) << "ARCVM not running, cannot share paths";
      std::move(callback).Run(base::FilePath(), false,
                              "ARCVM not running, cannot share paths");
      return;
    }
    request.set_handle(vm_info->seneschal_server_handle());
  } else {
    // Restart VM if not currently running.
    auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
    base::Optional<crostini::VmInfo> vm_info =
        crostini_manager->GetVmInfo(vm_name);
    if (!vm_info || vm_info->state != crostini::VmState::STARTED) {
      crostini_manager->RestartCrostini(
          crostini::ContainerId(vm_name,
                                crostini::kCrostiniDefaultContainerName),
          base::BindOnce(&OnVmRestartedForSeneschal, profile_, vm_name,
                         std::move(callback), std::move(request)));
      return;
    }
    request.set_handle(vm_info->info.seneschal_server_handle());
  }

  chromeos::DBusThreadManager::Get()->GetSeneschalClient()->SharePath(
      request,
      base::BindOnce(&OnSeneschalSharePathResponse, std::move(callback)));
}

void GuestOsSharePath::CallSeneschalUnsharePath(const std::string& vm_name,
                                                const base::FilePath& path,
                                                SuccessCallback callback) {
  vm_tools::seneschal::UnsharePathRequest request;

  // Return success if VM is not currently running.
  if (vm_name == plugin_vm::kPluginVmName) {
    if (plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)
            ->vm_state() !=
        vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING) {
      std::move(callback).Run(true, "PluginVm not running");
      return;
    }
    request.set_handle(
        plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)
            ->seneschal_server_handle());
  } else if (vm_name == arc::kArcVmName) {
    const auto& vm_info = arc::ArcSessionManager::Get()->GetVmInfo();
    if (!vm_info) {
      LOG(WARNING) << "ARCVM not running, cannot unshare paths";
      std::move(callback).Run(true, "ARCVM not running, cannot unshare paths");
      return;
    }
    request.set_handle(vm_info->seneschal_server_handle());
  } else {
    auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
    base::Optional<crostini::VmInfo> vm_info =
        crostini_manager->GetVmInfo(vm_name);
    if (!vm_info || vm_info->state != crostini::VmState::STARTED) {
      std::move(callback).Run(true, "VM not running");
      return;
    }
    request.set_handle(vm_info->info.seneschal_server_handle());
  }

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
        url::Origin(), storage::kFileSystemTypeExternal, virtual_path);
    result = file_manager::util::ConvertFileSystemURLToPathInsideVM(
        profile_, url, dummy_vm_mount,
        /*map_crostini_home=*/vm_name == crostini::kCrostiniDefaultVmName,
        &inside);
  }
  base::FilePath unshare_path;
  if (!result || !dummy_vm_mount.AppendRelativePath(inside, &unshare_path)) {
    std::move(callback).Run(false, "Invalid path to unshare");
    return;
  }

  request.set_path(unshare_path.value());
  chromeos::DBusThreadManager::Get()->GetSeneschalClient()->UnsharePath(
      request,
      base::BindOnce(&OnSeneschalUnsharePathResponse, std::move(callback)));
}

void GuestOsSharePath::SharePath(const std::string& vm_name,
                                 const base::FilePath& path,
                                 bool persist,
                                 SharePathCallback callback) {
  DCHECK(callback);
  CallSeneschalSharePath(vm_name, path, persist, std::move(callback));
}

void GuestOsSharePath::SharePaths(const std::string& vm_name,
                                  std::vector<base::FilePath> paths,
                                  bool persist,
                                  SuccessCallback callback) {
  base::RepeatingCallback<void(const base::FilePath&, const base::FilePath&,
                               bool, const std::string&)>
      barrier = base::BindRepeating(
          &ErrorCapture::Run,
          base::Owned(new ErrorCapture(paths.size(), std::move(callback))));
  for (const auto& path : paths) {
    CallSeneschalSharePath(vm_name, path, persist,
                           base::BindOnce(barrier, path));
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
    DictionaryPrefUpdate update(pref_service, prefs::kGuestOSPathsSharedToVms);
    base::DictionaryValue* shared_paths = update.Get();
    RemovePersistedPathFromPrefs(shared_paths, vm_name, path);
  }

  CallSeneschalUnsharePath(vm_name, path, std::move(callback));
  for (Observer& observer : observers_) {
    observer.OnUnshare(vm_name, path);
  }
}

bool GuestOsSharePath::GetAndSetFirstForSession() {
  bool result = first_for_session_;
  first_for_session_ = false;
  return result;
}

std::vector<base::FilePath> GuestOsSharePath::GetPersistedSharedPaths(
    const std::string& vm_name) {
  std::vector<base::FilePath> result;
  // TODO(crbug.com/1057591): Unexpected crashes here.
  CHECK(profile_);
  CHECK(profile_->GetPrefs());
  // |shared_paths| format is {'path': ['vm1', vm2']}.
  const base::DictionaryValue* shared_paths =
      profile_->GetPrefs()->GetDictionary(prefs::kGuestOSPathsSharedToVms);
  CHECK(shared_paths);
  for (const auto& it : shared_paths->DictItems()) {
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
                                           SuccessCallback callback) {
  SharePaths(vm_name, GetPersistedSharedPaths(vm_name),
             /*persist=*/false, std::move(callback));
}

void GuestOsSharePath::RegisterPersistedPath(const std::string& vm_name,
                                             const base::FilePath& path) {
  PrefService* pref_service = profile_->GetPrefs();
  DictionaryPrefUpdate update(pref_service, prefs::kGuestOSPathsSharedToVms);
  base::DictionaryValue* shared_paths = update.Get();
  // Check if path is already shared so we know whether we need to add it.
  bool already_shared = false;
  // Remove any paths that are children of this path.
  // E.g. if path /foo/bar is already shared, and then we share /foo, we
  // remove /foo/bar from the list since it will be shared as part of /foo.
  std::vector<base::FilePath> children;
  for (const auto& it : shared_paths->DictItems()) {
    base::FilePath shared(it.first);
    auto& vms = it.second;
    auto vm_matches = base::Contains(vms.GetList(), base::Value(vm_name));
    if (path == shared) {
      already_shared = true;
      if (!vm_matches) {
        vms.Append(vm_name);
      }
    } else if (path.IsParent(shared) && vm_matches) {
      children.emplace_back(shared);
    }
  }
  for (const auto& child : children) {
    RemovePersistedPathFromPrefs(shared_paths, vm_name, child);
  }
  if (!already_shared) {
    base::Value vms(base::Value::Type::LIST);
    vms.Append(base::Value(vm_name));
    shared_paths->SetKey(path.value(), std::move(vms));
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

void GuestOsSharePath::OnVmShutdown(const std::string& vm_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto it = shared_paths_.begin(); it != shared_paths_.end();) {
    if (RemoveSharedPathInfo(it->second, vm_name)) {
      shared_paths_.erase(it++);
    } else {
      ++it;
    }
  }
}

void GuestOsSharePath::OnVolumeMounted(chromeos::MountError error_code,
                                       const file_manager::Volume& volume) {
  if (error_code != chromeos::MountError::MOUNT_ERROR_NONE) {
    return;
  }

  // Check if any persisted paths match volume.mount_path() or are children
  // of it then share them with any running VMs.
  const base::DictionaryValue* shared_paths =
      profile_->GetPrefs()->GetDictionary(prefs::kGuestOSPathsSharedToVms);
  for (const auto& it : shared_paths->DictItems()) {
    base::FilePath path(it.first);
    if (path != volume.mount_path() && !volume.mount_path().IsParent(path)) {
      continue;
    }
    const auto& vms = it.second.GetList();
    for (const auto& vm : vms) {
      RegisterSharedPath(vm.GetString(), path);
      if (crostini::CrostiniManager::GetForProfile(profile_)->IsVmRunning(
              vm.GetString())) {
        CallSeneschalSharePath(
            vm.GetString(), path, false,
            base::BindOnce(seneschal_callback_, "share-on-mount", path));
      }
    }
  }
}

void GuestOsSharePath::OnVolumeUnmounted(chromeos::MountError error_code,
                                         const file_manager::Volume& volume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error_code != chromeos::MountError::MOUNT_ERROR_NONE) {
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

}  // namespace guest_os
