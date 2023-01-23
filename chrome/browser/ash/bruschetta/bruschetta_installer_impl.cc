// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_installer_impl.h"

#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/bruschetta/bruschetta_download_client.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
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
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/background_service/download_params.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace bruschetta {

// Also referenced by BruschettaInstallerTest.
extern const char kInstallResultMetric[] = "Bruschetta.InstallResult";

namespace {

const net::NetworkTrafficAnnotationTag kBruschettaTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("bruschetta_installer_download",
                                        R"(
      semantics {
        sender: "Bruschetta VM Installer",
        description: "Request sent to download firmware and VM image for "
          "a Bruschetta VM, which allows the user to run the VM."
        trigger: "User installing a Bruschetta VM"
        internal {
          contacts {
            email: "clumptini+oncall@google.com"
          }
        }
        user_data: {
          type: ACCESS_TOKEN
        }
        data: "Request to download Bruschetta firmware and VM image. "
          "Sends cookies associated with the source to authenticate the user."
        destination: WEBSITE
        last_reviewed: "2023-01-09"
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

std::unique_ptr<BruschettaInstallerImpl::Fds> OpenFdsBlocking(
    base::FilePath firmware_path,
    base::FilePath boot_disk_path,
    base::FilePath pflash_path,
    base::FilePath profile_path);

}  // namespace

struct BruschettaInstallerImpl::Fds {
  base::ScopedFD firmware;
  base::ScopedFD boot_disk;
  base::ScopedFD pflash;
};

BruschettaInstallerImpl::BruschettaInstallerImpl(
    Profile* profile,
    base::OnceClosure close_closure)
    : profile_(profile), close_closure_(std::move(close_closure)) {
  BruschettaDownloadClient::SetInstallerInstance(this);
}

BruschettaInstallerImpl::~BruschettaInstallerImpl() {
  BruschettaDownloadClient::SetInstallerInstance(nullptr);
}

bool BruschettaInstallerImpl::MaybeClose() {
  if (!install_running_) {
    if (close_closure_) {
      std::move(close_closure_).Run();
    }
    return true;
  }
  return false;
}

void BruschettaInstallerImpl::Cancel() {
  if (download_guid_.is_valid()) {
    BackgroundDownloadServiceFactory::GetForKey(profile_->GetProfileKey())
        ->CancelDownload(download_guid_.AsLowercaseString());
  }

  if (MaybeClose()) {
    return;
  }

  install_running_ = false;
}

void BruschettaInstallerImpl::Install(std::string vm_name,
                                      std::string config_id) {
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
    Error(BruschettaInstallResult::kInstallationProhibited);
    LOG(ERROR) << "Installation prohibited by policy";
    return;
  }
}

void BruschettaInstallerImpl::InstallToolsDlc() {
  VLOG(2) << "Installing DLC";
  NotifyObserver(State::kDlcInstall);

  dlcservice::InstallRequest request;
  request.set_id(kToolsDlc);
  ash::DlcserviceClient::Get()->Install(
      request,
      base::BindOnce(&BruschettaInstallerImpl::OnToolsDlcInstalled,
                     weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing());
}

void BruschettaInstallerImpl::OnToolsDlcInstalled(
    const ash::DlcserviceClient::InstallResult& install_result) {
  if (MaybeClose()) {
    return;
  }

  if (install_result.error != dlcservice::kErrorNone) {
    install_running_ = false;
    Error(BruschettaInstallResult::kDlcInstallError);
    LOG(ERROR) << "Failed to install tools dlc: " << install_result.error;
    return;
  }

  DownloadFirmware();
}

void BruschettaInstallerImpl::StartDownload(GURL url,
                                            DownloadCallback callback) {
  VLOG(2) << "Downloading " << url;
  auto* download_service =
      BackgroundDownloadServiceFactory::GetForKey(profile_->GetProfileKey());

  download::DownloadParams params;

  params.client = download::DownloadClient::BRUSCHETTA;

  params.guid = download_guid_.AsLowercaseString();
  params.callback = base::BindOnce(&BruschettaInstallerImpl::DownloadStarted,
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

void BruschettaInstallerImpl::DownloadStarted(
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

void BruschettaInstallerImpl::DownloadFailed() {
  download_guid_ = base::GUID();
  download_callback_.Reset();

  if (MaybeClose()) {
    return;
  }

  install_running_ = false;
  Error(BruschettaInstallResult::kDownloadError);
}

void BruschettaInstallerImpl::DownloadSucceeded(
    const download::CompletionInfo& completion_info) {
  download_guid_ = base::GUID();
  std::move(download_callback_).Run(completion_info);
}

void BruschettaInstallerImpl::DownloadFirmware() {
  VLOG(2) << "Downloading firmware";
  // We need to generate the download GUID before notifying because the tests
  // need it to set the response.
  download_guid_ = base::GUID::GenerateRandomV4();
  NotifyObserver(State::kFirmwareDownload);

  const std::string* url =
      config_.FindDict(prefs::kPolicyUefiKey)->FindString(prefs::kPolicyURLKey);
  StartDownload(GURL(*url),
                base::BindOnce(&BruschettaInstallerImpl::OnFirmwareDownloaded,
                               weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstallerImpl::OnFirmwareDownloaded(
    const download::CompletionInfo& completion_info) {
  if (MaybeClose()) {
    return;
  }

  const std::string* expected_hash = config_.FindDict(prefs::kPolicyUefiKey)
                                         ->FindString(prefs::kPolicyHashKey);
  if (!base::EqualsCaseInsensitiveASCII(completion_info.hash256,
                                        *expected_hash)) {
    install_running_ = false;
    Error(BruschettaInstallResult::kInvalidFirmware);
    LOG(ERROR) << "Downloaded firmware image has incorrect hash";
    LOG(ERROR) << "Actual   " << completion_info.hash256;
    LOG(ERROR) << "Expected " << *expected_hash;
    return;
  }

  firmware_path_ = completion_info.path;

  DownloadBootDisk();
}

void BruschettaInstallerImpl::DownloadBootDisk() {
  VLOG(2) << "Downloading boot disk";
  // We need to generate the download GUID before notifying because the tests
  // need it to set the response.
  download_guid_ = base::GUID::GenerateRandomV4();
  NotifyObserver(State::kBootDiskDownload);

  const std::string* url = config_.FindDict(prefs::kPolicyImageKey)
                               ->FindString(prefs::kPolicyURLKey);
  StartDownload(GURL(*url),
                base::BindOnce(&BruschettaInstallerImpl::OnBootDiskDownloaded,
                               weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstallerImpl::OnBootDiskDownloaded(
    const download::CompletionInfo& completion_info) {
  if (MaybeClose()) {
    return;
  }

  const std::string* expected_hash = config_.FindDict(prefs::kPolicyImageKey)
                                         ->FindString(prefs::kPolicyHashKey);
  if (!base::EqualsCaseInsensitiveASCII(completion_info.hash256,
                                        *expected_hash)) {
    install_running_ = false;
    Error(BruschettaInstallResult::kInvalidBootDisk);
    LOG(ERROR) << "Downloaded boot disk has incorrect hash";
    LOG(ERROR) << "Actual   " << completion_info.hash256;
    LOG(ERROR) << "Expected " << *expected_hash;
    return;
  }

  boot_disk_path_ = completion_info.path;

  DownloadPflash();
}

void BruschettaInstallerImpl::DownloadPflash() {
  VLOG(2) << "Downloading pflash";
  // We need to generate the download GUID before notifying because the tests
  // need it to set the response.
  download_guid_ = base::GUID::GenerateRandomV4();
  NotifyObserver(State::kPflashDownload);

  const std::string* url = config_.FindDict(prefs::kPolicyPflashKey)
                               ->FindString(prefs::kPolicyURLKey);
  StartDownload(GURL(*url),
                base::BindOnce(&BruschettaInstallerImpl::OnPflashDownloaded,
                               weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstallerImpl::OnPflashDownloaded(
    const download::CompletionInfo& completion_info) {
  if (MaybeClose()) {
    return;
  }

  const std::string* expected_hash = config_.FindDict(prefs::kPolicyPflashKey)
                                         ->FindString(prefs::kPolicyHashKey);
  if (!base::EqualsCaseInsensitiveASCII(completion_info.hash256,
                                        *expected_hash)) {
    install_running_ = false;
    Error(BruschettaInstallResult::kInvalidPflash);
    LOG(ERROR) << "Downloaded pflash has incorrect hash";
    LOG(ERROR) << "Actual   " << completion_info.hash256;
    LOG(ERROR) << "Expected " << *expected_hash;
    return;
  }

  pflash_path_ = completion_info.path;

  OpenFds();
}

void BruschettaInstallerImpl::OpenFds() {
  VLOG(2) << "Opening fds";
  NotifyObserver(State::kOpenFiles);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&OpenFdsBlocking, firmware_path_, boot_disk_path_,
                     pflash_path_, profile_->GetPath()),
      base::BindOnce(&BruschettaInstallerImpl::OnOpenFds,
                     weak_ptr_factory_.GetWeakPtr()));
}

namespace {

std::unique_ptr<BruschettaInstallerImpl::Fds> OpenFdsBlocking(
    base::FilePath firmware_path,
    base::FilePath boot_disk_path,
    base::FilePath pflash_path,
    base::FilePath profile_path) {
  auto firmware_dest_path = profile_path.Append(kBiosPath);
  VLOG(2) << "Copying " << firmware_path << " -> " << firmware_dest_path;
  if (!base::CopyFile(firmware_path, firmware_dest_path)) {
    PLOG(ERROR) << "Failed to move firmware image to destination";
    return nullptr;
  }

  auto pflash_dest_path = profile_path.Append(kPflashPath);
  if (!base::CopyFile(pflash_path, pflash_dest_path)) {
    PLOG(ERROR) << "Failed to move pflash image to destination";
    return nullptr;
  }

  base::File firmware(firmware_dest_path,
                      base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!firmware.IsValid()) {
    PLOG(ERROR) << "Failed to open firmware";
    return nullptr;
  }
  base::File boot_disk(boot_disk_path,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!boot_disk.IsValid()) {
    PLOG(ERROR) << "Failed to open boot disk";
    return nullptr;
  }
  base::File pflash(pflash_dest_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!pflash.IsValid()) {
    PLOG(ERROR) << "Failed to open pflash";
    return nullptr;
  }
  BruschettaInstallerImpl::Fds fds{
      .firmware = base::ScopedFD(firmware.TakePlatformFile()),
      .boot_disk = base::ScopedFD(boot_disk.TakePlatformFile()),
      .pflash = base::ScopedFD(pflash.TakePlatformFile())};
  return std::make_unique<BruschettaInstallerImpl::Fds>(std::move(fds));
}
}  // namespace

void BruschettaInstallerImpl::OnOpenFds(std::unique_ptr<Fds> fds) {
  if (MaybeClose()) {
    return;
  }

  if (!fds) {
    install_running_ = false;
    Error(BruschettaInstallResult::kUnableToOpenImages);
    LOG(ERROR) << "Failed to open image files";
    return;
  }

  fds_ = std::move(fds);

  CreateVmDisk();
}

void BruschettaInstallerImpl::CreateVmDisk() {
  VLOG(2) << "Creating VM disk";
  NotifyObserver(State::kCreateVmDisk);

  auto* client = ash::ConciergeClient::Get();
  DCHECK(client) << "This code requires a ConciergeClient";

  std::string user_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_);

  vm_tools::concierge::CreateDiskImageRequest request;

  request.set_cryptohome_id(std::move(user_hash));
  request.set_vm_name(kBruschettaVmName);
  request.set_image_type(vm_tools::concierge::DiskImageType::DISK_IMAGE_AUTO);

  client->CreateDiskImage(
      request, base::BindOnce(&BruschettaInstallerImpl::OnCreateVmDisk,
                              weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstallerImpl::OnCreateVmDisk(
    absl::optional<vm_tools::concierge::CreateDiskImageResponse> result) {
  if (MaybeClose()) {
    return;
  }

  if (!result ||
      result->status() !=
          vm_tools::concierge::DiskImageStatus::DISK_STATUS_CREATED) {
    install_running_ = false;
    Error(BruschettaInstallResult::kCreateDiskError);
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

void BruschettaInstallerImpl::StartVm() {
  VLOG(2) << "Starting VM";
  NotifyObserver(State::kStartVm);

  if (!GetInstallableConfig(profile_, config_id_)) {
    // Policy has changed to prohibit installation, so bail out before actually
    // starting the VM.
    install_running_ = false;
    Error(BruschettaInstallResult::kInstallationProhibited);
    LOG(ERROR) << "Installation prohibited by policy";
    return;
  }

  auto* client = ash::ConciergeClient::Get();
  DCHECK(client) << "This code requires a ConciergeClient";

  std::string user_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_);
  vm_tools::concierge::StartVmRequest request;

  request.set_name(kBruschettaVmName);
  request.set_owner_id(std::move(user_hash));
  request.mutable_vm()->set_tools_dlc_id(kToolsDlc);
  request.set_start_termina(false);

  auto* disk = request.add_disks();
  disk->set_path(std::move(disk_path_));
  disk->set_writable(true);

  request.add_oem_strings("com.google.glinux.installer.arg:track=latest");
  request.add_oem_strings("com.google.glinux.bruschetta.alpha");
  request.set_timeout(240);

  // fds and request.fds must have the same order.
  std::vector<base::ScopedFD> fds;
  request.add_fds(vm_tools::concierge::StartVmRequest::BIOS);
  fds.push_back(std::move(fds_->firmware));
  request.add_fds(vm_tools::concierge::StartVmRequest::STORAGE);
  fds.push_back(std::move(fds_->boot_disk));
  request.add_fds(vm_tools::concierge::StartVmRequest::PFLASH);
  fds.push_back(std::move(fds_->pflash));
  fds_.reset();

  client->StartVmWithFds(std::move(fds), request,
                         base::BindOnce(&BruschettaInstallerImpl::OnStartVm,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstallerImpl::OnStartVm(
    absl::optional<vm_tools::concierge::StartVmResponse> result) {
  if (MaybeClose()) {
    return;
  }

  if (!result || !result->success()) {
    install_running_ = false;
    Error(BruschettaInstallResult::kStartVmFailed);
    if (result) {
      LOG(ERROR) << "VM failed to start: " << result->failure_reason();
    } else {
      LOG(ERROR) << "VM failed to start, no response";
    }
    return;
  }

  LaunchTerminal();
}

void BruschettaInstallerImpl::LaunchTerminal() {
  VLOG(2) << "Launching terminal";
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
  base::UmaHistogramEnumeration(kInstallResultMetric,
                                BruschettaInstallResult::kSuccess);
  std::move(close_closure_).Run();
}

void BruschettaInstallerImpl::NotifyObserver(State state) {
  if (observer_) {
    observer_->StateChanged(state);
  }
}

void BruschettaInstallerImpl::Error(BruschettaInstallResult error) {
  VLOG(2) << "Error installing: " << BruschettaInstallResultString(error);
  base::UmaHistogramEnumeration(kInstallResultMetric, error);
  if (observer_) {
    observer_->Error(error);
  }
}

const base::GUID& BruschettaInstallerImpl::GetDownloadGuid() const {
  return download_guid_;
}

void BruschettaInstallerImpl::AddObserver(Observer* observer) {
  // We only support a single observer for now, since we'll only ever have one
  // (the UI calling us).
  DCHECK(observer_ == nullptr);
  observer_ = observer;
}

void BruschettaInstallerImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer_ == observer);
  observer_ = nullptr;
}

}  // namespace bruschetta
