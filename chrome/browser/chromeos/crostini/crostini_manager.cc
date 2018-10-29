// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_remover.h"
#include "chrome/browser/chromeos/crostini/crostini_reporting_util.h"
#include "chrome/browser/chromeos/crostini/crostini_share_path.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/image_loader_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/component_updater/component_updater_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/message.h"
#include "extensions/browser/extension_registry.h"
#include "net/base/escape.h"
#include "net/base/network_change_notifier.h"
#include "storage/browser/fileapi/external_mount_points.h"

namespace crostini {

namespace {

constexpr int64_t kMinimumDiskSize = 1ll * 1024 * 1024 * 1024;  // 1 GiB
constexpr base::FilePath::CharType kHomeDirectory[] =
    FILE_PATH_LITERAL("/home");
const char kSeparator[] = "--";

chromeos::CiceroneClient* GetCiceroneClient() {
  return chromeos::DBusThreadManager::Get()->GetCiceroneClient();
}

chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

void OnConciergeServiceAvailable(
    CrostiniManager::StartConciergeCallback callback,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Concierge service did not become available";
    std::move(callback).Run(success);
    return;
  }
  VLOG(1) << "Concierge service announced availability";
  VLOG(1) << "Waiting for Cicerone to announce availability.";

  GetCiceroneClient()->WaitForServiceToBeAvailable(std::move(callback));
}

}  // namespace

class CrostiniManager::CrostiniRestarter
    : public base::RefCountedThreadSafe<CrostiniRestarter>,
      public chromeos::disks::DiskMountManager::Observer {
 public:
  CrostiniRestarter(Profile* profile,
                    base::WeakPtr<CrostiniManager> crostini_manager,
                    std::string vm_name,
                    std::string container_name,
                    std::string container_username,
                    CrostiniManager::RestartCrostiniCallback callback)
      : profile_(profile),
        crostini_manager_(crostini_manager),
        vm_name_(std::move(vm_name)),
        container_name_(std::move(container_name)),
        container_username_(std::move(container_username)),
        callback_(std::move(callback)),
        restart_id_(next_restart_id_++) {}

  void Restart() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (is_aborted_)
      return;
    is_running_ = true;
    // Skip to the end immediately if testing.
    if (crostini_manager_->skip_restart_for_testing()) {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(&CrostiniRestarter::SetUpLxdContainerUserFinished,
                         base::WrapRefCounted(this), CrostiniResult::SUCCESS));
      return;
    }

    crostini_manager_->InstallTerminaComponent(base::BindOnce(
        &CrostiniRestarter::LoadComponentFinished, base::WrapRefCounted(this)));
  }

  void AddObserver(CrostiniManager::RestartObserver* observer) {
    observer_list_.AddObserver(observer);
  }

  void RunCallback(CrostiniResult result) { std::move(callback_).Run(result); }

  void Abort() {
    is_aborted_ = true;
    observer_list_.Clear();
  }

  void OnContainerDownloading(int download_percent) {
    if (!is_running_) {
      return;
    }
    // Tell observers.
    for (auto& observer : observer_list_) {
      observer.OnContainerDownloading(download_percent);
    }
  }

  CrostiniManager::RestartId restart_id() const { return restart_id_; }
  std::string vm_name() const { return vm_name_; }
  std::string container_name() const { return container_name_; }

 private:
  friend class base::RefCountedThreadSafe<CrostiniRestarter>;

  ~CrostiniRestarter() override {
    if (callback_) {
      LOG(ERROR) << "Destroying without having called the callback.";
    }
  }

  void FinishRestart(CrostiniResult result) {
    crostini_manager_->FinishRestart(this, result);
  }

  void LoadComponentFinished(CrostiniResult result) {
    // Tell observers.
    for (auto& observer : observer_list_) {
      observer.OnComponentLoaded(result);
    }
    if (is_aborted_)
      return;
    if (result != CrostiniResult::SUCCESS) {
      FinishRestart(result);
      return;
    }
    crostini_manager_->StartConcierge(
        base::BindOnce(&CrostiniRestarter::ConciergeStarted, this));
  }

  void ConciergeStarted(bool is_started) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CrostiniResult result = is_started ? CrostiniResult::SUCCESS
                                       : CrostiniResult::CONTAINER_START_FAILED;
    // Tell observers.
    for (auto& observer : observer_list_) {
      observer.OnConciergeStarted(result);
    }
    if (is_aborted_)
      return;
    if (!is_started) {
      LOG(ERROR) << "Failed to start Concierge service.";
      FinishRestart(result);
      return;
    }
    crostini_manager_->CreateDiskImage(
        base::FilePath(vm_name_),
        vm_tools::concierge::StorageLocation::STORAGE_CRYPTOHOME_ROOT,
        base::BindOnce(&CrostiniRestarter::CreateDiskImageFinished, this));
  }

  void CreateDiskImageFinished(CrostiniResult result,
                               const base::FilePath& result_path) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Tell observers.
    for (auto& observer : observer_list_) {
      observer.OnDiskImageCreated(result);
    }
    if (is_aborted_)
      return;
    if (result != CrostiniResult::SUCCESS) {
      LOG(ERROR) << "Failed to create disk image.";
      FinishRestart(result);
      return;
    }
    crostini_manager_->StartTerminaVm(
        vm_name_, result_path,
        base::BindOnce(&CrostiniRestarter::StartTerminaVmFinished, this));
  }

  void StartTerminaVmFinished(CrostiniResult result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Tell observers.
    for (auto& observer : observer_list_) {
      observer.OnVmStarted(result);
    }
    if (is_aborted_)
      return;
    if (result != CrostiniResult::SUCCESS) {
      LOG(ERROR) << "Failed to Start Termina VM.";
      FinishRestart(result);
      return;
    }
    crostini_manager_->CreateLxdContainer(
        vm_name_, container_name_,
        base::BindOnce(&CrostiniRestarter::CreateLxdContainerFinished, this));
  }

  void CreateLxdContainerFinished(CrostiniResult result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Tell observers.
    for (auto& observer : observer_list_) {
      observer.OnContainerCreated(result);
    }
    if (is_aborted_)
      return;
    if (result != CrostiniResult::SUCCESS) {
      LOG(ERROR) << "Failed to Create Lxd Container.";
      FinishRestart(result);
      return;
    }
    crostini_manager_->StartLxdContainer(
        vm_name_, container_name_,
        base::BindOnce(&CrostiniRestarter::StartLxdContainerFinished, this));
  }

  void StartLxdContainerFinished(CrostiniResult result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Tell observers.
    for (auto& observer : observer_list_) {
      observer.OnContainerStarted(result);
    }
    if (is_aborted_)
      return;
    if (result != CrostiniResult::SUCCESS) {
      LOG(ERROR) << "Failed to Start Lxd Container.";
      FinishRestart(result);
      return;
    }
    crostini_manager_->SetUpLxdContainerUser(
        vm_name_, container_name_, container_username_,
        base::BindOnce(&CrostiniRestarter::SetUpLxdContainerUserFinished,
                       this));
  }

  void SetUpLxdContainerUserFinished(CrostiniResult result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Tell observers.
    for (auto& observer : observer_list_) {
      observer.OnContainerSetup(result);
    }
    if (is_aborted_)
      return;
    if (result != CrostiniResult::SUCCESS) {
      LOG(ERROR) << "Failed to set up Lxd Container user.";
      FinishRestart(result);
      return;
    }

    // If default termina/penguin, then do sshfs mount and reshare folders,
    // else we are finished.
    if (vm_name_ == kCrostiniDefaultVmName &&
        container_name_ == kCrostiniDefaultContainerName) {
      crostini_manager_->GetContainerSshKeys(
          vm_name_, container_name_,
          base::BindOnce(&CrostiniRestarter::GetContainerSshKeysFinished,
                         this));
    } else {
      FinishRestart(result);
    }
  }

  void GetContainerSshKeysFinished(crostini::CrostiniResult result,
                                   const std::string& container_public_key,
                                   const std::string& host_private_key,
                                   const std::string& hostname) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Tell observers.
    for (auto& observer : observer_list_) {
      observer.OnSshKeysFetched(result);
    }
    if (is_aborted_)
      return;
    if (result != crostini::CrostiniResult::SUCCESS) {
      LOG(ERROR) << "Failed to get ssh keys.";
      FinishRestart(result);
      return;
    }

    // Add DiskMountManager::OnMountEvent observer.
    auto* dmgr = chromeos::disks::DiskMountManager::GetInstance();
    dmgr->AddObserver(this);

    // Call to sshfs to mount.
    source_path_ = base::StringPrintf(
        "sshfs://%s@%s:", container_username_.c_str(), hostname.c_str());
    dmgr->MountPath(source_path_, "",
                    file_manager::util::GetCrostiniMountPointName(profile_),
                    file_manager::util::GetCrostiniMountOptions(
                        hostname, host_private_key, container_public_key),
                    chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                    chromeos::MOUNT_ACCESS_MODE_READ_WRITE);
  }

  void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                    chromeos::MountError error_code,
                    const chromeos::disks::DiskMountManager::MountPointInfo&
                        mount_info) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Ignore any other mount/unmount events.
    if (event != chromeos::disks::DiskMountManager::MountEvent::MOUNTING ||
        mount_info.source_path != source_path_) {
      return;
    }
    // Remove DiskMountManager::OnMountEvent observer.
    chromeos::disks::DiskMountManager::GetInstance()->RemoveObserver(this);

    if (error_code != chromeos::MountError::MOUNT_ERROR_NONE) {
      LOG(ERROR) << "Error mounting crostini container: error_code="
                 << error_code << ", source_path=" << mount_info.source_path
                 << ", mount_path=" << mount_info.mount_path
                 << ", mount_type=" << mount_info.mount_type
                 << ", mount_condition=" << mount_info.mount_condition;
    } else {
      // Register filesystem and add volume to VolumeManager.
      base::FilePath mount_path = base::FilePath(mount_info.mount_path);
      storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
          file_manager::util::GetCrostiniMountPointName(profile_),
          storage::kFileSystemTypeNativeLocal, storage::FileSystemMountOption(),
          mount_path);

      // VolumeManager is null in unittest.
      if (auto* vmgr = file_manager::VolumeManager::Get(profile_))
        vmgr->AddSshfsCrostiniVolume(mount_path);
    }

    // Abort not checked until end of function.  On abort, do not continue,
    // but still remove observer and add volume as per above.
    if (is_aborted_)
      return;

    FinishRestart(CrostiniResult::SUCCESS);
  }

  Profile* profile_;
  // This isn't accessed after the CrostiniManager is destroyed, but we still
  // use a WeakPtr for safety.
  base::WeakPtr<CrostiniManager> crostini_manager_;

  std::string vm_name_;
  std::string container_name_;
  std::string container_username_;
  std::string source_path_;
  CrostiniManager::RestartCrostiniCallback callback_;
  base::ObserverList<CrostiniManager::RestartObserver>::Unchecked
      observer_list_;
  CrostiniManager::RestartId restart_id_;
  bool is_aborted_ = false;
  bool is_running_ = false;

  static CrostiniManager::RestartId next_restart_id_;
};

CrostiniManager::RestartId
    CrostiniManager::CrostiniRestarter::next_restart_id_ = 0;
bool CrostiniManager::is_cros_termina_registered_ = false;
// Unit tests need this initialized to true. In Browser tests and real life,
// it is updated via MaybeUpgradeCrostini.
bool CrostiniManager::is_dev_kvm_present_ = true;

void CrostiniManager::SetVmState(std::string vm_name, VmState vm_state) {
  auto vm_info = running_vms_.find(std::move(vm_name));
  if (vm_info != running_vms_.end()) {
    vm_info->second.first = vm_state;
    return;
  }
  // This can happen normally when StopVm is called right after start up.
  LOG(WARNING) << "Attempted to set state for unknown vm: " << vm_name;
}

bool CrostiniManager::IsVmRunning(std::string vm_name) {
  auto vm_info = running_vms_.find(std::move(vm_name));
  if (vm_info != running_vms_.end()) {
    return vm_info->second.first == VmState::STARTED;
  }
  return false;
}

base::Optional<vm_tools::concierge::VmInfo> CrostiniManager::GetVmInfo(
    std::string vm_name) {
  auto it = running_vms_.find(std::move(vm_name));
  if (it != running_vms_.end())
    return it->second.second;
  return base::nullopt;
}

void CrostiniManager::AddRunningVmForTesting(
    std::string vm_name,
    vm_tools::concierge::VmInfo vm_info) {
  running_vms_[std::move(vm_name)] =
      std::make_pair(VmState::STARTED, std::move(vm_info));
}

LinuxPackageInfo::LinuxPackageInfo() = default;
LinuxPackageInfo::~LinuxPackageInfo() = default;

bool CrostiniManager::IsContainerRunning(std::string vm_name,
                                         std::string container_name) {
  if (!IsVmRunning(vm_name)) {
    return false;
  }
  // TODO(jopra): Ensure the container not marked running if the vm is not
  // running.
  auto range = running_containers_.equal_range(std::move(vm_name));
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == container_name) {
      return true;
    }
  }
  return false;
}

void CrostiniManager::UpdateLaunchMetricsForEnterpriseReporting() {
  PrefService* const profile_prefs = profile_->GetPrefs();
  const component_updater::ComponentUpdateService* const update_service =
      g_browser_process->component_updater();
  const base::Clock* const clock = base::DefaultClock::GetInstance();
  WriteMetricsForReportingToPrefsIfEnabled(profile_prefs, update_service,
                                           clock);
}

CrostiniManager* CrostiniManager::GetForProfile(Profile* profile) {
  return CrostiniManagerFactory::GetForProfile(profile);
}

CrostiniManager::CrostiniManager(Profile* profile)
    : profile_(profile),
      owner_id_(CryptohomeIdForProfile(profile)),
      weak_ptr_factory_(this) {
  DCHECK(!profile_->IsOffTheRecord());
  GetCiceroneClient()->AddObserver(this);
  GetConciergeClient()->AddObserver(this);
}

CrostiniManager::~CrostiniManager() {
  GetCiceroneClient()->RemoveObserver(this);
  GetConciergeClient()->RemoveObserver(this);
}

// static
bool CrostiniManager::IsCrosTerminaInstalled() {
  return is_cros_termina_registered_;
}

// static
bool CrostiniManager::IsDevKvmPresent() {
  return is_dev_kvm_present_;
}

void CrostiniManager::MaybeUpgradeCrostini() {
  auto* component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  if (!component_manager) {
    // |component_manager| may be nullptr in unit tests.
    return;
  }
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(CrostiniManager::CheckPathsAndComponents),
      base::BindOnce(&CrostiniManager::MaybeUpgradeCrostiniAfterChecks,
                     weak_ptr_factory_.GetWeakPtr()));
}

// static
void CrostiniManager::CheckPathsAndComponents() {
  is_dev_kvm_present_ = base::PathExists(base::FilePath("/dev/kvm"));
  auto* component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  DCHECK(component_manager);
  is_cros_termina_registered_ =
      component_manager->IsRegistered(imageloader::kTerminaComponentName);
}

void CrostiniManager::MaybeUpgradeCrostiniAfterChecks() {
  if (!is_dev_kvm_present_) {
    return;
  }
  if (!is_cros_termina_registered_) {
    return;
  }
  if (!IsCrostiniAllowedForProfile(profile_)) {
    return;
  }
  termina_update_check_needed_ = true;
  if (net::NetworkChangeNotifier::IsOffline()) {
    // Can't do a component Load with kForce when offline.
    VLOG(1) << "Not online, so can't check now for cros-termina upgrade.";
    return;
  }
  InstallTerminaComponent(base::DoNothing());
}

void CrostiniManager::InstallTerminaComponent(CrostiniResultCallback callback) {
  auto* cros_component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  if (!cros_component_manager) {
    // Running in a unit test. We still PostTask to prevent races.
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&CrostiniManager::OnInstallTerminaComponent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       true, component_manager_load_error_for_testing_,
                       base::FilePath()));
    return;
  }

  DCHECK(cros_component_manager);

  bool major_update_required =
      is_cros_termina_registered_ &&
      cros_component_manager
          ->GetCompatiblePath(imageloader::kTerminaComponentName)
          .empty();
  bool is_offline = net::NetworkChangeNotifier::IsOffline();

  if (major_update_required) {
    termina_update_check_needed_ = false;
    if (is_offline) {
      LOG(ERROR) << "Need to load a major component update, but we're offline.";
      // TODO(nverne): Show a dialog/notification here for online upgrade
      // required.
      std::move(callback).Run(CrostiniResult::OFFLINE_WHEN_UPGRADE_REQUIRED);
      return;
    }
  }

  using UpdatePolicy = component_updater::CrOSComponentManager::UpdatePolicy;
  UpdatePolicy update_policy;
  if (termina_update_check_needed_ && !is_offline) {
    // Don't use kForce all the time because it generates traffic to
    // ComponentUpdaterService. Also, it's only appropriate for minor version
    // updates. Not major version incompatiblility.
    update_policy = UpdatePolicy::kForce;
  } else {
    update_policy = UpdatePolicy::kDontForce;
  }

  cros_component_manager->Load(
      imageloader::kTerminaComponentName,
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      update_policy,
      base::BindOnce(&CrostiniManager::OnInstallTerminaComponent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     update_policy == UpdatePolicy::kForce));
}

void CrostiniManager::OnInstallTerminaComponent(
    CrostiniResultCallback callback,
    bool is_update_checked,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& result) {
  bool is_successful =
      error == component_updater::CrOSComponentManager::Error::NONE;

  if (is_successful) {
    is_cros_termina_registered_ = true;
  } else {
    LOG(ERROR)
        << "Failed to install the cros-termina component with error code: "
        << static_cast<int>(error);
  }

  if (is_successful && is_update_checked) {
    VLOG(1) << "cros-termina update check successful.";
    termina_update_check_needed_ = false;
  }

  std::move(callback).Run(is_successful
                              ? CrostiniResult::SUCCESS
                              : CrostiniResult::LOAD_COMPONENT_FAILED);
}

bool CrostiniManager::UninstallTerminaComponent() {
  bool success = true;
  auto* cros_component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  if (cros_component_manager) {
    success =
        cros_component_manager->Unload(imageloader::kTerminaComponentName);
  }
  if (success) {
    is_cros_termina_registered_ = false;
  }
  return success;
}

void CrostiniManager::StartConcierge(StartConciergeCallback callback) {
  VLOG(1) << "Starting Concierge service";
  chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->StartConcierge(
      base::BindOnce(&CrostiniManager::OnStartConcierge,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::OnStartConcierge(StartConciergeCallback callback,
                                       bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to start Concierge service";
    std::move(callback).Run(success);
    return;
  }
  VLOG(1) << "Concierge service started";
  VLOG(1) << "Waiting for Concierge to announce availability.";

  GetConciergeClient()->WaitForServiceToBeAvailable(
      base::BindOnce(&OnConciergeServiceAvailable, std::move(callback)));
}

void CrostiniManager::StopConcierge(StopConciergeCallback callback) {
  VLOG(1) << "Stopping Concierge service";
  chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->StopConcierge(
      base::BindOnce(&CrostiniManager::OnStopConcierge,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::OnStopConcierge(StopConciergeCallback callback,
                                      bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to stop Concierge service";
  } else {
    VLOG(1) << "Concierge service stopped";
  }
  std::move(callback).Run(success);
}

void CrostiniManager::CreateDiskImage(
    const base::FilePath& disk_path,
    vm_tools::concierge::StorageLocation storage_location,
    CreateDiskImageCallback callback) {
  std::string disk_path_string = disk_path.AsUTF8Unsafe();
  if (disk_path_string.empty()) {
    LOG(ERROR) << "Disk path cannot be empty";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR, base::FilePath());
    return;
  }

  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_cryptohome_id(CryptohomeIdForProfile(profile_));
  request.set_disk_path(std::move(disk_path_string));
  // The type of disk image to be created.
  request.set_image_type(vm_tools::concierge::DISK_IMAGE_QCOW2);

  if (storage_location != vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT &&
      storage_location != vm_tools::concierge::STORAGE_CRYPTOHOME_DOWNLOADS) {
    LOG(ERROR) << "'" << storage_location
               << "' is not a valid storage location";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR, base::FilePath());
    return;
  }
  request.set_storage_location(storage_location);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath(kHomeDirectory)),
      base::BindOnce(&CrostiniManager::CreateDiskImageAfterSizeCheck,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback)));
}

void CrostiniManager::CreateDiskImageAfterSizeCheck(
    vm_tools::concierge::CreateDiskImageRequest request,
    CreateDiskImageCallback callback,
    int64_t free_disk_size) {
  int64_t disk_size = (free_disk_size * 9) / 10;
  // Skip disk size check on dev box or trybots because
  // base::SysInfo::AmountOfFreeDiskSpace returns zero in testing.
  if (disk_size < kMinimumDiskSize && base::SysInfo::IsRunningOnChromeOS()) {
    LOG(ERROR) << "Insufficient disk available. Need to free "
               << kMinimumDiskSize - disk_size << " bytes";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR, base::FilePath());
    return;
  }
  // The logical size of the new disk image, in bytes.
  request.set_disk_size(std::move(disk_size));

  GetConciergeClient()->CreateDiskImage(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCreateDiskImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::DestroyDiskImage(
    const base::FilePath& disk_path,
    vm_tools::concierge::StorageLocation storage_location,
    DestroyDiskImageCallback callback) {
  std::string disk_path_string = disk_path.AsUTF8Unsafe();
  if (disk_path_string.empty()) {
    LOG(ERROR) << "Disk path cannot be empty";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }

  vm_tools::concierge::DestroyDiskImageRequest request;
  request.set_cryptohome_id(CryptohomeIdForProfile(profile_));
  request.set_disk_path(std::move(disk_path_string));

  if (storage_location != vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT &&
      storage_location != vm_tools::concierge::STORAGE_CRYPTOHOME_DOWNLOADS) {
    LOG(ERROR) << "'" << storage_location
               << "' is not a valid storage location";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  request.set_storage_location(storage_location);

  GetConciergeClient()->DestroyDiskImage(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnDestroyDiskImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::ListVmDisks(ListVmDisksCallback callback) {
  vm_tools::concierge::ListVmDisksRequest request;
  request.set_cryptohome_id(CryptohomeIdForProfile(profile_));
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);

  GetConciergeClient()->ListVmDisks(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnListVmDisks,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::StartTerminaVm(std::string name,
                                     const base::FilePath& disk_path,
                                     StartTerminaVmCallback callback) {
  if (name.empty()) {
    LOG(ERROR) << "name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }

  std::string disk_path_string = disk_path.AsUTF8Unsafe();
  if (disk_path_string.empty()) {
    LOG(ERROR) << "Disk path cannot be empty";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }

  vm_tools::concierge::StartVmRequest request;
  request.set_name(std::move(name));
  request.set_start_termina(true);
  request.set_owner_id(owner_id_);

  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(std::move(disk_path_string));
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_QCOW2);
  disk_image->set_writable(true);
  disk_image->set_do_mount(false);

  GetConciergeClient()->StartTerminaVm(
      request, base::BindOnce(&CrostiniManager::OnStartTerminaVm,
                              weak_ptr_factory_.GetWeakPtr(), request.name(),
                              std::move(callback)));
}

void CrostiniManager::StopVm(std::string name, StopVmCallback callback) {
  if (name.empty()) {
    LOG(ERROR) << "name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }

  SetVmState(name, VmState::STOPPING);

  vm_tools::concierge::StopVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_name(name);

  GetConciergeClient()->StopVm(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnStopVm, weak_ptr_factory_.GetWeakPtr(),
                     std::move(name), std::move(callback)));
}

void CrostiniManager::CreateLxdContainer(std::string vm_name,
                                         std::string container_name,
                                         CrostiniResultCallback callback) {
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (!GetCiceroneClient()->IsLxdContainerCreatedSignalConnected() ||
      !GetCiceroneClient()->IsLxdContainerDownloadingSignalConnected()) {
    LOG(ERROR)
        << "Async call to CreateLxdContainer can't complete when signals "
           "are not connected.";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  vm_tools::cicerone::CreateLxdContainerRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_owner_id(owner_id_);
  request.set_image_server(kCrostiniDefaultImageServerUrl);
  request.set_image_alias(kCrostiniDefaultImageAlias);
  GetCiceroneClient()->CreateLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCreateLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), request.vm_name(),
                     request.container_name(), std::move(callback)));
}

void CrostiniManager::StartLxdContainer(std::string vm_name,
                                        std::string container_name,
                                        CrostiniResultCallback callback) {
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (!GetCiceroneClient()->IsContainerStartedSignalConnected() ||
      !GetCiceroneClient()->IsContainerShutdownSignalConnected()) {
    LOG(ERROR) << "Async call to StartLxdContainer can't complete when signals "
                  "are not connected.";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  vm_tools::cicerone::StartLxdContainerRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_owner_id(owner_id_);
  GetCiceroneClient()->StartLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnStartLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), request.vm_name(),
                     request.container_name(), std::move(callback)));
}

void CrostiniManager::SetUpLxdContainerUser(std::string vm_name,
                                            std::string container_name,
                                            std::string container_username,
                                            CrostiniResultCallback callback) {
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_username.empty()) {
    LOG(ERROR) << "container_username is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  vm_tools::cicerone::SetUpLxdContainerUserRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_owner_id(owner_id_);
  request.set_container_username(std::move(container_username));
  GetCiceroneClient()->SetUpLxdContainerUser(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnSetUpLxdContainerUser,
                     weak_ptr_factory_.GetWeakPtr(), request.vm_name(),
                     request.container_name(), std::move(callback)));
}

void CrostiniManager::LaunchContainerApplication(
    std::string vm_name,
    std::string container_name,
    std::string desktop_file_id,
    const std::vector<std::string>& files,
    bool display_scaled,
    LaunchContainerApplicationCallback callback) {
  vm_tools::cicerone::LaunchContainerApplicationRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_desktop_file_id(std::move(desktop_file_id));
  if (display_scaled) {
    request.set_display_scaling(
        vm_tools::cicerone::LaunchContainerApplicationRequest::SCALED);
  }
  std::copy(
      files.begin(), files.end(),
      google::protobuf::RepeatedFieldBackInserter(request.mutable_files()));

  GetCiceroneClient()->LaunchContainerApplication(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnLaunchContainerApplication,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::GetContainerAppIcons(
    std::string vm_name,
    std::string container_name,
    std::vector<std::string> desktop_file_ids,
    int icon_size,
    int scale,
    GetContainerAppIconsCallback callback) {
  vm_tools::cicerone::ContainerAppIconRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  google::protobuf::RepeatedPtrField<std::string> ids(
      std::make_move_iterator(desktop_file_ids.begin()),
      std::make_move_iterator(desktop_file_ids.end()));
  request.mutable_desktop_file_ids()->Swap(&ids);
  request.set_size(icon_size);
  request.set_scale(scale);

  GetCiceroneClient()->GetContainerAppIcons(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnGetContainerAppIcons,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::GetLinuxPackageInfo(
    Profile* profile,
    std::string vm_name,
    std::string container_name,
    std::string package_path,
    GetLinuxPackageInfoCallback callback) {
  vm_tools::cicerone::LinuxPackageInfoRequest request;
  request.set_owner_id(CryptohomeIdForProfile(profile));
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_file_path(std::move(package_path));

  GetCiceroneClient()->GetLinuxPackageInfo(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnGetLinuxPackageInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::InstallLinuxPackage(
    std::string vm_name,
    std::string container_name,
    std::string package_path,
    InstallLinuxPackageCallback callback) {
  if (!GetCiceroneClient()->IsInstallLinuxPackageProgressSignalConnected()) {
    // Technically we could still start the install, but we wouldn't be able to
    // detect when the install completes, successfully or otherwise.
    LOG(ERROR)
        << "Attempted to install package when progress signal not connected.";
    std::move(callback).Run(CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED,
                            std::string());
    return;
  }

  vm_tools::cicerone::InstallLinuxPackageRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_file_path(std::move(package_path));

  GetCiceroneClient()->InstallLinuxPackage(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnInstallLinuxPackage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::GetContainerSshKeys(
    std::string vm_name,
    std::string container_name,
    GetContainerSshKeysCallback callback) {
  vm_tools::concierge::ContainerSshKeysRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_cryptohome_id(CryptohomeIdForProfile(profile_));

  GetConciergeClient()->GetContainerSshKeys(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnGetContainerSshKeys,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// static
GURL CrostiniManager::GenerateVshInCroshUrl(
    Profile* profile,
    const std::string& vm_name,
    const std::string& container_name,
    const std::vector<std::string>& terminal_args) {
  std::string vsh_crosh = base::StringPrintf(
      "chrome-extension://%s/html/crosh.html?command=vmshell",
      kCrostiniCroshBuiltinAppId);
  std::string vm_name_param = net::EscapeQueryParamValue(
      base::StringPrintf("--vm_name=%s", vm_name.c_str()), false);
  std::string container_name_param = net::EscapeQueryParamValue(
      base::StringPrintf("--target_container=%s", container_name.c_str()),
      false);
  std::string owner_id_param = net::EscapeQueryParamValue(
      base::StringPrintf("--owner_id=%s",
                         CryptohomeIdForProfile(profile).c_str()),
      false);

  std::vector<base::StringPiece> pieces = {
      vsh_crosh, vm_name_param, container_name_param, owner_id_param};
  if (!terminal_args.empty()) {
    // Separates the command args from the args we are passing into the
    // terminal to be executed.
    pieces.emplace_back(kSeparator);
    for (auto arg : terminal_args) {
      pieces.emplace_back(net::EscapeQueryParamValue(arg, false));
    }
  }

  GURL vsh_in_crosh_url(base::JoinString(pieces, "&args[]="));
  return vsh_in_crosh_url;
}

// static
AppLaunchParams CrostiniManager::GenerateTerminalAppLaunchParams(
    Profile* profile) {
  const extensions::Extension* crosh_extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          kCrostiniCroshBuiltinAppId);

  AppLaunchParams launch_params(
      profile, crosh_extension, extensions::LAUNCH_CONTAINER_WINDOW,
      WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_APP_LAUNCHER);
  launch_params.override_app_name =
      AppNameFromCrostiniAppId(kCrostiniTerminalId);
  return launch_params;
}

// static
Browser* CrostiniManager::CreateContainerTerminal(
    const AppLaunchParams& launch_params,
    const GURL& vsh_in_crosh_url) {
  return CreateApplicationWindow(launch_params, vsh_in_crosh_url);
}

// static
void CrostiniManager::ShowContainerTerminal(
    const AppLaunchParams& launch_params,
    const GURL& vsh_in_crosh_url,
    Browser* browser) {
  ShowApplicationWindow(launch_params, vsh_in_crosh_url, browser);
}

void CrostiniManager::LaunchContainerTerminal(
    const std::string& vm_name,
    const std::string& container_name,
    const std::vector<std::string>& terminal_args) {
  GURL vsh_in_crosh_url =
      GenerateVshInCroshUrl(profile_, vm_name, container_name, terminal_args);
  AppLaunchParams launch_params = GenerateTerminalAppLaunchParams(profile_);
  OpenApplicationWindow(launch_params, vsh_in_crosh_url);
}

CrostiniManager::RestartId CrostiniManager::RestartCrostini(
    std::string vm_name,
    std::string container_name,
    RestartCrostiniCallback callback,
    RestartObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto restarter = base::MakeRefCounted<CrostiniRestarter>(
      profile_, weak_ptr_factory_.GetWeakPtr(), std::move(vm_name),
      std::move(container_name), ContainerUserNameForProfile(profile_),
      std::move(callback));
  if (observer)
    restarter->AddObserver(observer);
  auto key = std::make_pair(restarter->vm_name(), restarter->container_name());
  restarters_by_container_.emplace(key, restarter->restart_id());
  restarters_by_id_[restarter->restart_id()] = restarter;
  if (restarters_by_container_.count(key) > 1) {
    VLOG(1) << "Already restarting vm " << vm_name << ", container "
            << container_name;
  } else {
    restarter->Restart();
  }
  return restarter->restart_id();
}

void CrostiniManager::AbortRestartCrostini(
    CrostiniManager::RestartId restart_id) {
  auto restarter_it = restarters_by_id_.find(restart_id);
  if (restarter_it == restarters_by_id_.end()) {
    // This can happen if a user cancels the install flow at the exact right
    // moment, for example.
    LOG(ERROR) << "Aborting a restarter that already finished";
    return;
  }
  restarter_it->second->Abort();

  auto key = std::make_pair(restarter_it->second->vm_name(),
                            restarter_it->second->container_name());
  auto range = restarters_by_container_.equal_range(key);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == restart_id) {
      restarters_by_container_.erase(it);
      break;
    }
  }

  // This invalidates the iterator and potentially destroys the restarter, so
  // those shouldn't be accessed after this.
  restarters_by_id_.erase(restarter_it);

  // Kick off the "next" (in no order) pending Restart() if any.
  auto pending_it = restarters_by_container_.find(key);
  if (pending_it != restarters_by_container_.end()) {
    auto restarter = restarters_by_id_[pending_it->second];
    restarter->Restart();
  }
}

bool CrostiniManager::IsRestartPending(RestartId restart_id) {
  return restarters_by_id_.find(restart_id) != restarters_by_id_.end();
}

void CrostiniManager::AddShutdownContainerCallback(
    std::string vm_name,
    std::string container_name,
    ShutdownContainerCallback shutdown_callback) {
  shutdown_container_callbacks_.emplace(
      std::make_tuple(vm_name, container_name), std::move(shutdown_callback));
}

void CrostiniManager::AddRemoveCrostiniCallback(
    RemoveCrostiniCallback remove_callback) {
  remove_crostini_callbacks_.emplace_back(std::move(remove_callback));
}

void CrostiniManager::AddInstallLinuxPackageProgressObserver(
    InstallLinuxPackageProgressObserver* observer) {
  install_linux_package_progress_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveInstallLinuxPackageProgressObserver(
    InstallLinuxPackageProgressObserver* observer) {
  install_linux_package_progress_observers_.RemoveObserver(observer);
}

void CrostiniManager::OnCreateDiskImage(
    CreateDiskImageCallback callback,
    base::Optional<vm_tools::concierge::CreateDiskImageResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to create disk image. Empty response.";
    std::move(callback).Run(CrostiniResult::CREATE_DISK_IMAGE_FAILED,
                            base::FilePath());
    return;
  }
  vm_tools::concierge::CreateDiskImageResponse response = reply.value();

  if (response.status() != vm_tools::concierge::DISK_STATUS_EXISTS &&
      response.status() != vm_tools::concierge::DISK_STATUS_CREATED) {
    LOG(ERROR) << "Failed to create disk image: " << response.failure_reason();
    std::move(callback).Run(CrostiniResult::CREATE_DISK_IMAGE_FAILED,
                            base::FilePath());
    return;
  }

  std::move(callback).Run(CrostiniResult::SUCCESS,
                          base::FilePath(response.disk_path()));
}

void CrostiniManager::OnDestroyDiskImage(
    DestroyDiskImageCallback callback,
    base::Optional<vm_tools::concierge::DestroyDiskImageResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to destroy disk image. Empty response.";
    std::move(callback).Run(CrostiniResult::DESTROY_DISK_IMAGE_FAILED);
    return;
  }
  vm_tools::concierge::DestroyDiskImageResponse response =
      std::move(reply).value();

  if (response.status() != vm_tools::concierge::DISK_STATUS_DESTROYED &&
      response.status() != vm_tools::concierge::DISK_STATUS_DOES_NOT_EXIST) {
    LOG(ERROR) << "Failed to destroy disk image: " << response.failure_reason();
    std::move(callback).Run(CrostiniResult::DESTROY_DISK_IMAGE_FAILED);
    return;
  }

  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::OnListVmDisks(
    ListVmDisksCallback callback,
    base::Optional<vm_tools::concierge::ListVmDisksResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to get list of VM disks. Empty response.";
    std::move(callback).Run(
        CrostiniResult::LIST_VM_DISKS_FAILED,
        profile_->GetPrefs()->GetInt64(prefs::kCrostiniLastDiskSize));
    return;
  }
  vm_tools::concierge::ListVmDisksResponse response = std::move(reply).value();

  if (!response.success()) {
    LOG(ERROR) << "Failed to list VM disks: " << response.failure_reason();
    std::move(callback).Run(
        CrostiniResult::LIST_VM_DISKS_FAILED,
        profile_->GetPrefs()->GetInt64(prefs::kCrostiniLastDiskSize));
    return;
  }

  profile_->GetPrefs()->SetInt64(prefs::kCrostiniLastDiskSize,
                                 response.total_size());
  std::move(callback).Run(CrostiniResult::SUCCESS, response.total_size());
}

void CrostiniManager::OnStartTerminaVm(
    std::string vm_name,
    StartTerminaVmCallback callback,
    base::Optional<vm_tools::concierge::StartVmResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to start termina vm. Empty response.";
    std::move(callback).Run(CrostiniResult::VM_START_FAILED);
    return;
  }
  vm_tools::concierge::StartVmResponse response = reply.value();

  if (response.status() == vm_tools::concierge::VM_STATUS_FAILURE ||
      response.status() == vm_tools::concierge::VM_STATUS_UNKNOWN) {
    LOG(ERROR) << "Failed to start VM: " << response.failure_reason();
    // If we thought vms and containers were running before, they aren't now.
    running_vms_.erase(vm_name);
    running_containers_.erase(vm_name);
    std::move(callback).Run(CrostiniResult::VM_START_FAILED);
    return;
  }

  // If the vm is already marked "running" run the callback.
  if (response.status() == vm_tools::concierge::VM_STATUS_RUNNING) {
    running_vms_[vm_name] =
        std::make_pair(VmState::STARTED, std::move(response.vm_info()));
    std::move(callback).Run(CrostiniResult::SUCCESS);
    return;
  }

  // Otherwise, record the container start and run the callback after the VM
  // starts.
  DCHECK_EQ(response.status(), vm_tools::concierge::VM_STATUS_STARTING);
  VLOG(1) << "Awaiting TremplinStartedSignal for " << owner_id_ << ", "
          << vm_name;
  running_vms_[vm_name] =
      std::make_pair(VmState::STARTING, std::move(response.vm_info()));
  // If we thought a container was running for this VM, we're wrong. This can
  // happen if the vm was formerly running, then stopped via crosh.
  running_containers_.erase(vm_name);

  tremplin_started_callbacks_.emplace(
      vm_name, base::BindOnce(&CrostiniManager::OnStartTremplin,
                              weak_ptr_factory_.GetWeakPtr(), vm_name,
                              std::move(callback), CrostiniResult::SUCCESS));

  // Share folders from Downloads, etc with default VM.
  if (vm_name == kCrostiniDefaultVmName) {
    SharePersistedPaths(profile_, base::DoNothing());
  }
}

void CrostiniManager::OnStartTremplin(std::string vm_name,
                                      StartTerminaVmCallback callback,
                                      CrostiniResult result) {
  // Record the running vm.
  VLOG(1) << "Received TremplinStartedSignal, VM: " << owner_id_ << ", "
          << vm_name;
  SetVmState(vm_name, VmState::STARTED);

  // Run the original callback.
  std::move(callback).Run(result);
}

void CrostiniManager::OnStopVm(
    std::string vm_name,
    StopVmCallback callback,
    base::Optional<vm_tools::concierge::StopVmResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to stop termina vm. Empty response.";
    std::move(callback).Run(CrostiniResult::VM_STOP_FAILED);
    return;
  }
  vm_tools::concierge::StopVmResponse response = reply.value();

  if (!response.success()) {
    LOG(ERROR) << "Failed to stop VM: " << response.failure_reason();
    // TODO(rjwright): Change the service so that "Requested VM does not
    // exist" is not an error. "Requested VM does not exist" means that there
    // is a disk image for the VM but it is not running, either because it has
    // not been started or it has already been stopped. There's no need for
    // this to be an error, and making it a success will save us having to
    // discriminate on failure_reason here.
    if (response.failure_reason() != "Requested VM does not exist") {
      std::move(callback).Run(CrostiniResult::VM_STOP_FAILED);
      return;
    }
  }
  // Remove from running_vms_.
  running_vms_.erase(vm_name);
  // Remove containers from running_containers_
  running_containers_.erase(std::move(vm_name));
  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::OnContainerStarted(
    const vm_tools::cicerone::ContainerStartedSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;
  // Find the callbacks to call, then erase them from the map.
  auto range = start_container_callbacks_.equal_range(
      std::make_tuple(signal.vm_name(), signal.container_name()));
  for (auto it = range.first; it != range.second; ++it) {
    std::move(it->second).Run(CrostiniResult::SUCCESS);
  }
  start_container_callbacks_.erase(range.first, range.second);
  running_containers_.emplace(signal.vm_name(), signal.container_name());
}

void CrostiniManager::OnContainerStartupFailed(
    const vm_tools::concierge::ContainerStartedSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;
  // Find the callbacks to call, then erase them from the map.
  auto range = start_container_callbacks_.equal_range(
      std::make_tuple(signal.vm_name(), signal.container_name()));
  for (auto it = range.first; it != range.second; ++it) {
    std::move(it->second).Run(CrostiniResult::CONTAINER_START_FAILED);
  }
  start_container_callbacks_.erase(range.first, range.second);
}

void CrostiniManager::OnContainerShutdown(
    const vm_tools::cicerone::ContainerShutdownSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;
  // Find the callbacks to call, then erase them from the map.
  auto range = shutdown_container_callbacks_.equal_range(
      std::make_tuple(signal.vm_name(), signal.container_name()));
  for (auto it = range.first; it != range.second; ++it) {
    std::move(it->second).Run();
  }
  shutdown_container_callbacks_.erase(range.first, range.second);
}

void CrostiniManager::OnInstallLinuxPackageProgress(
    const vm_tools::cicerone::InstallLinuxPackageProgressSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;
  if (signal.progress_percent() < 0 || signal.progress_percent() > 100) {
    LOG(ERROR) << "Received install progress with invalid progress of "
               << signal.progress_percent() << "%.";
    return;
  }

  InstallLinuxPackageProgressStatus status;
  switch (signal.status()) {
    case vm_tools::cicerone::InstallLinuxPackageProgressSignal::SUCCEEDED:
      status = InstallLinuxPackageProgressStatus::SUCCEEDED;
      break;
    case vm_tools::cicerone::InstallLinuxPackageProgressSignal::FAILED:
      status = InstallLinuxPackageProgressStatus::FAILED;
      break;
    case vm_tools::cicerone::InstallLinuxPackageProgressSignal::DOWNLOADING:
      status = InstallLinuxPackageProgressStatus::DOWNLOADING;
      break;
    case vm_tools::cicerone::InstallLinuxPackageProgressSignal::INSTALLING:
      status = InstallLinuxPackageProgressStatus::INSTALLING;
      break;
    default:
      NOTREACHED();
  }

  for (auto& observer : install_linux_package_progress_observers_) {
    observer.OnInstallLinuxPackageProgress(
        signal.vm_name(), signal.container_name(), status,
        signal.progress_percent(), signal.failure_details());
  }
}

void CrostiniManager::OnCreateLxdContainer(
    std::string vm_name,
    std::string container_name,
    CrostiniResultCallback callback,
    base::Optional<vm_tools::cicerone::CreateLxdContainerResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to create lxd container in vm. Empty response.";
    std::move(callback).Run(CrostiniResult::CONTAINER_START_FAILED);
    return;
  }
  vm_tools::cicerone::CreateLxdContainerResponse response = reply.value();
  if (response.status() ==
      vm_tools::cicerone::CreateLxdContainerResponse::CREATING) {
    VLOG(1) << "Awaiting LxdContainerCreatedSignal for " << owner_id_ << ", "
            << vm_name << ", " << container_name;
    // The callback will be called when we receive the LxdContainerCreated
    // signal.
    create_lxd_container_callbacks_.emplace(
        std::make_tuple(vm_name, container_name), std::move(callback));
    return;
  }
  if (response.status() !=
      vm_tools::cicerone::CreateLxdContainerResponse::EXISTS) {
    LOG(ERROR) << "Failed to start container: " << response.failure_reason();
    std::move(callback).Run(CrostiniResult::CONTAINER_START_FAILED);
    return;
  }
  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::OnStartLxdContainer(
    std::string vm_name,
    std::string container_name,
    CrostiniResultCallback callback,
    base::Optional<vm_tools::cicerone::StartLxdContainerResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to start lxd container in vm. Empty response.";
    std::move(callback).Run(CrostiniResult::CONTAINER_START_FAILED);
    return;
  }
  vm_tools::cicerone::StartLxdContainerResponse response = reply.value();

  if (!(response.status() ==
            vm_tools::cicerone::StartLxdContainerResponse::STARTED ||
        response.status() ==
            vm_tools::cicerone::StartLxdContainerResponse::RUNNING)) {
    LOG(ERROR) << "Failed to start container: " << response.failure_reason();
    std::move(callback).Run(CrostiniResult::CONTAINER_START_FAILED);
    return;
  }
  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::OnSetUpLxdContainerUser(
    std::string vm_name,
    std::string container_name,
    CrostiniResultCallback callback,
    base::Optional<vm_tools::cicerone::SetUpLxdContainerUserResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to set up lxd container user. Empty response.";
    std::move(callback).Run(CrostiniResult::CONTAINER_START_FAILED);
    return;
  }
  vm_tools::cicerone::SetUpLxdContainerUserResponse response = reply.value();

  if (!(response.status() ==
            vm_tools::cicerone::SetUpLxdContainerUserResponse::SUCCESS ||
        response.status() ==
            vm_tools::cicerone::SetUpLxdContainerUserResponse::EXISTS)) {
    LOG(ERROR) << "Failed to set up container user: "
               << response.failure_reason();
    std::move(callback).Run(CrostiniResult::CONTAINER_START_FAILED);
    return;
  }

  if (!IsContainerRunning(vm_name, container_name)) {
    start_container_callbacks_.emplace(std::make_tuple(vm_name, container_name),
                                       std::move(callback));
    return;
  }
  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::OnLxdContainerCreated(
    const vm_tools::cicerone::LxdContainerCreatedSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;
  CrostiniResult result;

  switch (signal.status()) {
    case vm_tools::cicerone::LxdContainerCreatedSignal::UNKNOWN:
      result = CrostiniResult::UNKNOWN_ERROR;
      break;
    case vm_tools::cicerone::LxdContainerCreatedSignal::CREATED:
      result = CrostiniResult::SUCCESS;
      break;
    case vm_tools::cicerone::LxdContainerCreatedSignal::DOWNLOAD_TIMED_OUT:
      result = CrostiniResult::CONTAINER_DOWNLOAD_TIMED_OUT;
      break;
    case vm_tools::cicerone::LxdContainerCreatedSignal::CANCELLED:
      result = CrostiniResult::CONTAINER_CREATE_CANCELLED;
      break;
    case vm_tools::cicerone::LxdContainerCreatedSignal::FAILED:
      result = CrostiniResult::CONTAINER_CREATE_FAILED;
      break;
    default:
      result = CrostiniResult::UNKNOWN_ERROR;
      break;
  }
  // Find the callbacks to call, then erase them from the map.
  auto range = create_lxd_container_callbacks_.equal_range(
      std::make_tuple(signal.vm_name(), signal.container_name()));
  for (auto it = range.first; it != range.second; ++it) {
    std::move(it->second).Run(result);
  }
  create_lxd_container_callbacks_.erase(range.first, range.second);
}

void CrostiniManager::OnLxdContainerDownloading(
    const vm_tools::cicerone::LxdContainerDownloadingSignal& signal) {
  if (owner_id_ != signal.owner_id()) {
    return;
  }
  auto range = restarters_by_container_.equal_range(
      std::make_pair(signal.vm_name(), signal.container_name()));
  for (auto it = range.first; it != range.second; ++it) {
    restarters_by_id_[it->second]->OnContainerDownloading(
        signal.download_progress());
  }
}

void CrostiniManager::OnTremplinStarted(
    const vm_tools::cicerone::TremplinStartedSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;
  // Find the callbacks to call, then erase them from the map.
  auto range = tremplin_started_callbacks_.equal_range(signal.vm_name());
  for (auto it = range.first; it != range.second; ++it) {
    std::move(it->second).Run();
  }
  tremplin_started_callbacks_.erase(range.first, range.second);
}

void CrostiniManager::OnLaunchContainerApplication(
    LaunchContainerApplicationCallback callback,
    base::Optional<vm_tools::cicerone::LaunchContainerApplicationResponse>
        reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to launch application. Empty response.";
    std::move(callback).Run(
        CrostiniResult::LAUNCH_CONTAINER_APPLICATION_FAILED);
    return;
  }
  vm_tools::cicerone::LaunchContainerApplicationResponse response =
      reply.value();

  if (!response.success()) {
    LOG(ERROR) << "Failed to launch application: " << response.failure_reason();
    std::move(callback).Run(
        CrostiniResult::LAUNCH_CONTAINER_APPLICATION_FAILED);
    return;
  }
  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::OnGetContainerAppIcons(
    GetContainerAppIconsCallback callback,
    base::Optional<vm_tools::cicerone::ContainerAppIconResponse> reply) {
  std::vector<Icon> icons;
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to get container application icons. Empty response.";
    std::move(callback).Run(CrostiniResult::DBUS_ERROR, icons);
    return;
  }
  vm_tools::cicerone::ContainerAppIconResponse response = reply.value();
  for (auto& icon : *response.mutable_icons()) {
    icons.emplace_back(
        Icon{.desktop_file_id = std::move(*icon.mutable_desktop_file_id()),
             .content = std::move(*icon.mutable_icon())});
  }
  std::move(callback).Run(CrostiniResult::SUCCESS, icons);
}

void CrostiniManager::OnGetLinuxPackageInfo(
    GetLinuxPackageInfoCallback callback,
    base::Optional<vm_tools::cicerone::LinuxPackageInfoResponse> reply) {
  LinuxPackageInfo result;
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to get Linux package info. Empty response.";
    result.success = false;
    // The error message is currently only used in a console message. If we
    // want to display it to the user, we'd need to localize this.
    result.failure_reason = "D-Bus response was empty.";
    std::move(callback).Run(result);
    return;
  }
  vm_tools::cicerone::LinuxPackageInfoResponse response = reply.value();

  if (!response.success()) {
    LOG(ERROR) << "Failed to get Linux package info: "
               << response.failure_reason();
    result.success = false;
    result.failure_reason = response.failure_reason();
    std::move(callback).Run(result);
    return;
  }

  // The |package_id| field is formatted like "name;version;arch;data". We're
  // currently only interested in name and version.
  std::vector<std::string> split = base::SplitString(
      response.package_id(), ";", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (split.size() < 2 || split[0].empty() || split[1].empty()) {
    LOG(ERROR) << "Linux package info contained invalid package id: \""
               << response.package_id() << '"';
    result.success = false;
    result.failure_reason = "Linux package info contained invalid package id.";
    std::move(callback).Run(result);
    return;
  }

  result.success = true;
  result.name = split[0];
  result.version = split[1];
  result.description = response.description();
  result.summary = response.summary();

  std::move(callback).Run(result);
}

void CrostiniManager::OnInstallLinuxPackage(
    InstallLinuxPackageCallback callback,
    base::Optional<vm_tools::cicerone::InstallLinuxPackageResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to install Linux package. Empty response.";
    std::move(callback).Run(CrostiniResult::LAUNCH_CONTAINER_APPLICATION_FAILED,
                            std::string());
    return;
  }
  vm_tools::cicerone::InstallLinuxPackageResponse response = reply.value();

  if (response.status() ==
      vm_tools::cicerone::InstallLinuxPackageResponse::FAILED) {
    LOG(ERROR) << "Failed to install Linux package: "
               << response.failure_reason();
    std::move(callback).Run(CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED,
                            response.failure_reason());
    return;
  }

  if (response.status() ==
      vm_tools::cicerone::InstallLinuxPackageResponse::INSTALL_ALREADY_ACTIVE) {
    LOG(WARNING) << "Failed to install Linux package, install already active.";
    std::move(callback).Run(
        CrostiniResult::INSTALL_LINUX_PACKAGE_ALREADY_ACTIVE, std::string());
    return;
  }

  std::move(callback).Run(CrostiniResult::SUCCESS, std::string());
}

void CrostiniManager::OnGetContainerSshKeys(
    GetContainerSshKeysCallback callback,
    base::Optional<vm_tools::concierge::ContainerSshKeysResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to get ssh keys. Empty response.";
    std::move(callback).Run(CrostiniResult::DBUS_ERROR, "", "", "");
    return;
  }
  vm_tools::concierge::ContainerSshKeysResponse response = reply.value();
  std::move(callback).Run(CrostiniResult::SUCCESS,
                          response.container_public_key(),
                          response.host_private_key(), response.hostname());
}

void CrostiniManager::RemoveCrostini(std::string vm_name,
                                     std::string container_name,
                                     RemoveCrostiniCallback callback) {
  AddRemoveCrostiniCallback(std::move(callback));
  auto crostini_remover = base::MakeRefCounted<CrostiniRemover>(
      profile_, std::move(vm_name), std::move(container_name),
      base::BindOnce(&CrostiniManager::OnRemoveCrostini,
                     weak_ptr_factory_.GetWeakPtr()));
  crostini_remover->RemoveCrostini();
}

void CrostiniManager::OnRemoveCrostini(CrostiniResult result) {
  for (auto& callback : remove_crostini_callbacks_) {
    std::move(callback).Run(result);
  }
  remove_crostini_callbacks_.clear();
}

void CrostiniManager::FinishRestart(CrostiniRestarter* restarter,
                                    CrostiniResult result) {
  auto key = std::make_pair(restarter->vm_name(), restarter->container_name());
  auto range = restarters_by_container_.equal_range(key);
  std::vector<scoped_refptr<CrostiniRestarter>> pending_restarters;
  // Erase first, because restarter->RunCallback() may modify our maps.
  for (auto it = range.first; it != range.second; ++it) {
    CrostiniManager::RestartId restart_id = it->second;
    pending_restarters.emplace_back(restarters_by_id_[restart_id]);
    restarters_by_id_.erase(restart_id);
  }
  restarters_by_container_.erase(range.first, range.second);
  for (const auto& pending_restarter : pending_restarters) {
    pending_restarter->RunCallback(result);
  }
}

}  // namespace crostini
