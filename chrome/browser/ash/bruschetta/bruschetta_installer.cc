// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/bruschetta/bruschetta_download_client.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/background_service/download_params.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace bruschetta {

namespace {

const net::NetworkTrafficAnnotationTag kBruschettaTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("bruschetta_installer_download",
                                        R"(
      semantics {
        sender: "Bruschetta VM Installer",
        description: "Request sent to download firmware and VM image for "
          "a Bruschetta VM, which allows the user to run the VM."
        trigger: "User installing a Bruschetta VM"
        user_data: {
          type: ACCESS_TOKEN
        }
        data: "Request to download Bruschetta firmware and VM image. "
          "Sends cookies associated with the source to authenticate the user."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        chrome_policy {
          BruschettaVMConfiguration {
            BruschettaVMConfiguration: "{}"
          }
        }
      }
    )");

absl::optional<std::pair<base::ScopedFD, base::ScopedFD>> OpenFdsBlocking(
    base::FilePath firmware_path,
    base::FilePath boot_disk_path,
    base::FilePath profile_path);

}  // namespace

BruschettaInstaller::BruschettaInstaller(Profile* profile,
                                         base::OnceClosure close_closure)
    : profile_(profile), close_closure_(std::move(close_closure)) {
  BruschettaDownloadClient::SetInstallerInstance(this);
}

BruschettaInstaller::~BruschettaInstaller() {
  BruschettaDownloadClient::SetInstallerInstance(nullptr);
}

bool BruschettaInstaller::MaybeClose() {
  if (!install_running_) {
    std::move(close_closure_).Run();
    return true;
  }
  return false;
}

void BruschettaInstaller::Cancel() {
  if (download_guid_.is_valid()) {
    BackgroundDownloadServiceFactory::GetForKey(profile_->GetProfileKey())
        ->CancelDownload(download_guid_.AsLowercaseString());
  }

  if (MaybeClose())
    return;

  install_running_ = false;
}

void BruschettaInstaller::Install(std::string vm_name, std::string config_id) {
  if (install_running_) {
    LOG(ERROR) << "Install requested while an install is already running";
    return;
  }

  NotifyObserver(State::kInstallStarted);

  install_running_ = true;

  auto config_ptr = GetInstallableConfig(profile_, config_id);
  if (config_ptr.has_value()) {
    config_ = config_ptr.value()->Clone();
    config_id_ = std::move(config_id);
    vm_name_ = std::move(vm_name);
    InstallToolsDlc();
  } else {
    install_running_ = false;
    NotifyObserverError();
    LOG(ERROR) << "Installation prohibited by policy";
    return;
  }
}

void BruschettaInstaller::InstallToolsDlc() {
  NotifyObserver(State::kDlcInstall);

  dlcservice::InstallRequest request;
  request.set_id(crostini::kCrostiniDlcName);
  chromeos::DlcserviceClient::Get()->Install(
      request,
      base::BindOnce(&BruschettaInstaller::OnToolsDlcInstalled,
                     weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing());
}

void BruschettaInstaller::OnToolsDlcInstalled(
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  if (MaybeClose())
    return;

  if (install_result.error != dlcservice::kErrorNone) {
    install_running_ = false;
    NotifyObserverError();
    LOG(ERROR) << "Failed to install tools dlc: " << install_result.error;
    return;
  }

  DownloadFirmware();
}

void BruschettaInstaller::StartDownload(GURL url, DownloadCallback callback) {
  auto* download_service =
      BackgroundDownloadServiceFactory::GetForKey(profile_->GetProfileKey());

  download::DownloadParams params;

  params.client = download::DownloadClient::BRUSCHETTA;

  params.guid = download_guid_.AsLowercaseString();
  params.callback = base::BindOnce(&BruschettaInstaller::DownloadStarted,
                                   weak_ptr_factory_.GetWeakPtr());

  download_callback_ = std::move(callback);

  params.scheduling_params.priority = download::SchedulingParams::Priority::UI;
  params.scheduling_params.network_requirements =
      download::SchedulingParams::NetworkRequirements::NONE;
  params.scheduling_params.battery_requirements =
      download::SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;

  params.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(kBruschettaTrafficAnnotation);

  params.request_params.url = std::move(url);

  download_service->StartDownload(std::move(params));
}

void BruschettaInstaller::DownloadStarted(
    const std::string& guid,
    download::DownloadParams::StartResult result) {
  if (guid != download_guid_.AsLowercaseString()) {
    LOG(ERROR) << "Got unexpected response from download service";
    return;
  }

  if (result != download::DownloadParams::StartResult::ACCEPTED) {
    LOG(ERROR) << "Download failed to start, error code " << result;
    DownloadFailed();
  }
}

void BruschettaInstaller::DownloadFailed() {
  download_guid_ = base::GUID();
  download_callback_.Reset();

  if (MaybeClose()) {
    return;
  }

  install_running_ = false;
  NotifyObserverError();
}

void BruschettaInstaller::DownloadSucceeded(
    const download::CompletionInfo& completion_info) {
  download_guid_ = base::GUID();
  std::move(download_callback_).Run(completion_info);
}

void BruschettaInstaller::DownloadFirmware() {
  // We need to generate the download GUID before notifying because the tests
  // need it to set the response.
  download_guid_ = base::GUID::GenerateRandomV4();
  NotifyObserver(State::kFirmwareDownload);

  const std::string* url =
      config_.FindDict(prefs::kPolicyUefiKey)->FindString(prefs::kPolicyURLKey);
  StartDownload(GURL(*url),
                base::BindOnce(&BruschettaInstaller::OnFirmwareDownloaded,
                               weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstaller::OnFirmwareDownloaded(
    const download::CompletionInfo& completion_info) {
  if (MaybeClose())
    return;

  const std::string* expected_hash = config_.FindDict(prefs::kPolicyUefiKey)
                                         ->FindString(prefs::kPolicyHashKey);
  if (!base::EqualsCaseInsensitiveASCII(completion_info.hash256,
                                        *expected_hash)) {
    install_running_ = false;
    NotifyObserverError();
    LOG(ERROR) << "Downloaded firmware image has incorrect hash";
    LOG(ERROR) << "Actual   " << completion_info.hash256;
    LOG(ERROR) << "Expected " << *expected_hash;
    return;
  }

  MountFirmware(completion_info.path);
}

void BruschettaInstaller::MountFirmware(const base::FilePath& path) {
  NotifyObserver(State::kFirmwareMount);

  ash::disks::DiskMountManager::GetInstance()->MountPath(
      path.AsUTF8Unsafe(), "", "", {}, ash::MountType::kArchive,
      ash::MountAccessMode::kReadOnly,
      base::BindOnce(&BruschettaInstaller::OnFirmwareMounted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstaller::OnFirmwareMounted(ash::MountError error_code,
                                            const ash::MountPoint& mount_info) {
  if (MaybeClose())
    return;

  if (error_code != ash::MountError::kSuccess) {
    install_running_ = false;
    NotifyObserverError();
    LOG(ERROR) << "Failed to unpack firmware image: " << error_code;
    return;
  }

  firmware_mount_path_ = mount_info.mount_path;

  DownloadBootDisk();
}

void BruschettaInstaller::DownloadBootDisk() {
  // We need to generate the download GUID before notifying because the tests
  // need it to set the response.
  download_guid_ = base::GUID::GenerateRandomV4();
  NotifyObserver(State::kBootDiskDownload);

  const std::string* url = config_.FindDict(prefs::kPolicyImageKey)
                               ->FindString(prefs::kPolicyURLKey);
  StartDownload(GURL(*url),
                base::BindOnce(&BruschettaInstaller::OnBootDiskDownloaded,
                               weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstaller::OnBootDiskDownloaded(
    const download::CompletionInfo& completion_info) {
  if (MaybeClose())
    return;

  const std::string* expected_hash = config_.FindDict(prefs::kPolicyImageKey)
                                         ->FindString(prefs::kPolicyHashKey);
  if (!base::EqualsCaseInsensitiveASCII(completion_info.hash256,
                                        *expected_hash)) {
    install_running_ = false;
    NotifyObserverError();
    LOG(ERROR) << "Downloaded boot disk has incorrect hash";
    LOG(ERROR) << "Actual   " << completion_info.hash256;
    LOG(ERROR) << "Expected " << *expected_hash;
    return;
  }

  MountBootDisk(completion_info.path);
}

void BruschettaInstaller::MountBootDisk(const base::FilePath& path) {
  NotifyObserver(State::kBootDiskMount);

  ash::disks::DiskMountManager::GetInstance()->MountPath(
      path.AsUTF8Unsafe(), "", "", {}, ash::MountType::kArchive,
      ash::MountAccessMode::kReadOnly,
      base::BindOnce(&BruschettaInstaller::OnBootDiskMounted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstaller::OnBootDiskMounted(ash::MountError error_code,
                                            const ash::MountPoint& mount_info) {
  if (MaybeClose())
    return;

  if (error_code != ash::MountError::kSuccess) {
    install_running_ = false;
    NotifyObserverError();
    LOG(ERROR) << "Failed to unpack firmware image: " << error_code;
    return;
  }

  boot_disk_mount_path_ = mount_info.mount_path;

  OpenFds();
}

void BruschettaInstaller::OpenFds() {
  NotifyObserver(State::kOpenFiles);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&OpenFdsBlocking, base::FilePath(firmware_mount_path_),
                     base::FilePath(boot_disk_mount_path_),
                     profile_->GetPath()),
      base::BindOnce(&BruschettaInstaller::OnOpenFds,
                     weak_ptr_factory_.GetWeakPtr()));
}

namespace {

absl::optional<base::FilePath> FindPath(const base::FilePath& dir) {
  auto enumerator =
      base::FileEnumerator(dir, true, base::FileEnumerator::FILES);
  auto filepath = enumerator.Next();
  if (filepath.empty()) {
    LOG(ERROR) << "No files under mount point";
    return absl::nullopt;
  }
  if (!enumerator.Next().empty()) {
    LOG(ERROR) << "Multiple files under mount point";
    return absl::nullopt;
  }
  return filepath;
}

absl::optional<std::pair<base::ScopedFD, base::ScopedFD>> OpenFdsBlocking(
    base::FilePath firmware_path,
    base::FilePath boot_disk_path,
    base::FilePath profile_path) {
  auto firmware_src_path = FindPath(firmware_path);
  if (!firmware_src_path) {
    LOG(ERROR) << "Couldn't find firmware image";
    return absl::nullopt;
  }

  auto boot_disk_file = FindPath(boot_disk_path);
  if (!boot_disk_file) {
    LOG(ERROR) << "Couldn't find boot disk";
    return absl::nullopt;
  }

  auto firmware_dest_path = profile_path.Append(kBiosPath);

  if (!base::CopyFile(*firmware_src_path, firmware_dest_path)) {
    PLOG(ERROR) << "Failed to move firmware image to destination";
    return absl::nullopt;
  }

  base::File firmware(firmware_dest_path,
                      base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File boot_disk(*boot_disk_file,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!firmware.IsValid() || !boot_disk.IsValid()) {
    PLOG(ERROR) << "Failed to open boot disk or firmware image";
    return absl::nullopt;
  }

  return std::pair(base::ScopedFD(firmware.TakePlatformFile()),
                   base::ScopedFD(boot_disk.TakePlatformFile()));
}
}  // namespace

void BruschettaInstaller::OnOpenFds(
    absl::optional<std::pair<base::ScopedFD, base::ScopedFD>> fds) {
  if (MaybeClose())
    return;

  if (!fds) {
    install_running_ = false;
    NotifyObserverError();
    LOG(ERROR) << "Failed to open image files";
    return;
  }

  firmware_fd_ = std::move(fds->first);
  boot_disk_fd_ = std::move(fds->second);

  CreateVmDisk();
}

void BruschettaInstaller::CreateVmDisk() {
  NotifyObserver(State::kCreateVmDisk);

  auto* client = chromeos::ConciergeClient::Get();
  DCHECK(client) << "This code requires a ConciergeClient";

  std::string user_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_);

  vm_tools::concierge::CreateDiskImageRequest request;

  request.set_cryptohome_id(std::move(user_hash));
  request.set_vm_name(kBruschettaVmName);
  request.set_image_type(vm_tools::concierge::DiskImageType::DISK_IMAGE_AUTO);

  client->CreateDiskImage(request,
                          base::BindOnce(&BruschettaInstaller::OnCreateVmDisk,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstaller::OnCreateVmDisk(
    absl::optional<vm_tools::concierge::CreateDiskImageResponse> result) {
  if (MaybeClose())
    return;

  if (!result ||
      result->status() !=
          vm_tools::concierge::DiskImageStatus::DISK_STATUS_CREATED) {
    install_running_ = false;
    NotifyObserverError();
    if (result) {
      LOG(ERROR) << "Create VM failed: " << result->failure_reason();
    } else {
      LOG(ERROR) << "Create VM failed, no response";
    }
    return;
  }

  disk_path_ = result->disk_path();

  StartVm();
}

void BruschettaInstaller::StartVm() {
  NotifyObserver(State::kStartVm);

  if (!GetInstallableConfig(profile_, config_id_)) {
    // Policy has changed to prohibit installation, so bail out before actually
    // starting the VM.
    install_running_ = false;
    NotifyObserverError();
    LOG(ERROR) << "Installation prohibited by policy";
    return;
  }

  auto* client = chromeos::ConciergeClient::Get();
  DCHECK(client) << "This code requires a ConciergeClient";

  std::string user_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_);
  vm_tools::concierge::StartVmRequest request;

  request.set_name(kBruschettaVmName);
  request.set_owner_id(std::move(user_hash));
  request.mutable_vm()->set_tools_dlc_id("termina-dlc");
  request.set_start_termina(false);

  auto* disk = request.add_disks();
  disk->set_path(std::move(disk_path_));
  disk->set_writable(true);

  request.add_kernel_params("biosdevname=0");
  request.add_kernel_params("net.ifnames=0");
  request.add_kernel_params("console=hvc0");
  request.add_kernel_params("earlycon=uart8250,io,0x3f8");
  request.add_kernel_params("g-i/track=latest");
  request.add_kernel_params("glinux/bruschetta-alpha");
  request.set_timeout(240);

  // fds and request.fds must have the same order.
  std::vector<base::ScopedFD> fds;
  request.add_fds(vm_tools::concierge::StartVmRequest::BIOS);
  fds.push_back(std::move(firmware_fd_));
  request.add_fds(vm_tools::concierge::StartVmRequest::ROOTFS);
  fds.push_back(std::move(boot_disk_fd_));

  client->StartVmWithFds(std::move(fds), request,
                         base::BindOnce(&BruschettaInstaller::OnStartVm,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstaller::OnStartVm(
    absl::optional<vm_tools::concierge::StartVmResponse> result) {
  if (MaybeClose())
    return;

  if (!result || !result->success()) {
    install_running_ = false;
    NotifyObserverError();
    if (result) {
      LOG(ERROR) << "VM failed to start: " << result->failure_reason();
    } else {
      LOG(ERROR) << "VM failed to start, no response";
    }
    return;
  }

  LaunchTerminal();
}

void BruschettaInstaller::LaunchTerminal() {
  NotifyObserver(State::kLaunchTerminal);

  // TODO(b/231899688): Implement Bruschetta sending an RPC when installation
  // finishes so that we only add to prefs on success.
  auto guest_id = MakeBruschettaId(std::move(vm_name_));
  BruschettaService::GetForProfile(profile_)->RegisterInPrefs(
      guest_id, std::move(config_id_));

  guest_id.container_name = "";

  // kInvalidDisplayId will launch terminal on the current active display.
  guest_os::LaunchTerminal(profile_, display::kInvalidDisplayId, guest_id);

  // Close dialog.
  std::move(close_closure_).Run();
}

void BruschettaInstaller::NotifyObserver(State state) {
  if (observer_)
    observer_->StateChanged(state);
}

void BruschettaInstaller::NotifyObserverError() {
  if (observer_)
    observer_->Error();
}

const base::GUID& BruschettaInstaller::GetDownloadGuid() const {
  return download_guid_;
}

}  // namespace bruschetta
