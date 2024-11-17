// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_installer_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "bruschetta_installer.h"
#include "chrome/browser/ash/bruschetta/bruschetta_download.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/pref_service.h"

namespace bruschetta {

// Also referenced by BruschettaInstallerTest.
extern const char kInstallResultMetric[] = "Bruschetta.InstallResult";

namespace {

// The vTPM EK key label.
// Should be synced with the value in the chromiumos repo:
// src/platform2/vtpm/backends/attested_virtual_endorsement.cc
constexpr char kVtpmEkLabel[] = "vtpm-ek";
constexpr uint64_t kBruschettaRequiredMemory =
    12ULL * 1024 * 1024 * 1024;  // 12 GiB

std::unique_ptr<BruschettaInstallerImpl::Fds> OpenFdsBlocking(
    base::FilePath boot_disk_path,
    base::FilePath pflash_path,
    base::FilePath profile_path);

}  // namespace

struct BruschettaInstallerImpl::Fds {
  base::ScopedFD boot_disk;
  std::optional<base::ScopedFD> pflash;
};

BruschettaInstallerImpl::BruschettaInstallerImpl(
    Profile* profile,
    base::OnceClosure close_closure)
    : profile_(profile), close_closure_(std::move(close_closure)) {}

BruschettaInstallerImpl::~BruschettaInstallerImpl() = default;

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
  if (MaybeClose()) {
    return;
  }

  install_running_ = false;
}

void BruschettaInstallerImpl::Install(std::string vm_name,
                                      std::string config_id) {
  if (!base::FeatureList::IsEnabled(
          ash::features::kDisableBruschettaInstallChecks)) {
    uint64_t physical_memory = base::SysInfo::AmountOfPhysicalMemory();
    // Physical memory reporting never lines up with exact GB definitions, allow
    // for some wiggle room.
    if (physical_memory < 0.85 * kBruschettaRequiredMemory) {
      Error(BruschettaInstallResult::kNotEnoughMemoryError);
      LOG(ERROR) << "System memory of " << physical_memory
                 << " less than required " << kBruschettaRequiredMemory;
      return;
    }
    const std::optional<std::string_view> attested_device_id =
        ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
            ash::system::kAttestedDeviceIdKey);
    if (!attested_device_id.has_value()) {
      Error(BruschettaInstallResult::kNoAdidError);
      LOG(ERROR) << "No ADID is available";
      return;
    }
  }

  if (install_running_) {
    LOG(ERROR) << "Install requested while an install is already running";
    return;
  }

  auto new_guest_id = MakeBruschettaId(config_id);
  for (const auto& guest_id :
       guest_os::GetContainers(profile_, guest_os::VmType::BRUSCHETTA)) {
    if (guest_id == new_guest_id) {
      Error(BruschettaInstallResult::kVmAlreadyExists);
      LOG(ERROR) << "Tried to install a VM that already exists";
      return;
    }
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

  // Reset mic permissions, as these should not persist across reinstall.
  profile_->GetPrefs()->SetBoolean(prefs::kBruschettaMicAllowed, false);
}

void BruschettaInstallerImpl::InstallToolsDlc() {
  VLOG(2) << "Installing tools DLC";
  NotifyObserver(State::kToolsDlcInstall);

  in_progress_dlc_ = std::make_unique<guest_os::GuestOsDlcInstallation>(
      kToolsDlc,
      base::BindOnce(&BruschettaInstallerImpl::OnToolsDlcInstalled,
                     weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing());
}

void BruschettaInstallerImpl::OnToolsDlcInstalled(
    guest_os::GuestOsDlcInstallation::Result install_result) {
  in_progress_dlc_.reset();

  if (MaybeClose()) {
    return;
  }

  if (!install_result.has_value()) {
    install_running_ = false;
    BruschettaInstallResult result;
    switch (install_result.error()) {
      case guest_os::GuestOsDlcInstallation::Error::Offline:
        result = BruschettaInstallResult::kToolsDlcOfflineError;
        break;
      case guest_os::GuestOsDlcInstallation::Error::NeedUpdate:
        result = BruschettaInstallResult::kToolsDlcNeedUpdateError;
        break;
      case guest_os::GuestOsDlcInstallation::Error::NeedReboot:
        result = BruschettaInstallResult::kToolsDlcNeedRebootError;
        break;
      case guest_os::GuestOsDlcInstallation::Error::DiskFull:
        result = BruschettaInstallResult::kToolsDlcDiskFullError;
        break;
      case guest_os::GuestOsDlcInstallation::Error::Busy:
        result = BruschettaInstallResult::kToolsDlcBusyError;
        break;
      case guest_os::GuestOsDlcInstallation::Error::Internal:
      case guest_os::GuestOsDlcInstallation::Error::Invalid:
      case guest_os::GuestOsDlcInstallation::Error::UnknownFailure:
      case guest_os::GuestOsDlcInstallation::Error::Cancelled:
      default:
        result = BruschettaInstallResult::kToolsDlcUnknownError;
        break;
    }
    Error(result);
    LOG(ERROR) << "Failed to install tools dlc: " << install_result.error();
    return;
  }

  InstallFirmwareDlc();
}

void BruschettaInstallerImpl::InstallFirmwareDlc() {
  VLOG(2) << "Installing firmware DLC";
  NotifyObserver(State::kFirmwareDlcInstall);

  in_progress_dlc_ = std::make_unique<guest_os::GuestOsDlcInstallation>(
      kUefiDlc,
      base::BindOnce(&BruschettaInstallerImpl::OnFirmwareDlcInstalled,
                     weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing());
}

void BruschettaInstallerImpl::OnFirmwareDlcInstalled(
    guest_os::GuestOsDlcInstallation::Result install_result) {
  if (MaybeClose()) {
    return;
  }

  if (!install_result.has_value()) {
    install_running_ = false;
    BruschettaInstallResult result;
    switch (install_result.error()) {
      case guest_os::GuestOsDlcInstallation::Error::Offline:
        result = BruschettaInstallResult::kFirmwareDlcOfflineError;
        break;
      case guest_os::GuestOsDlcInstallation::Error::NeedUpdate:
        result = BruschettaInstallResult::kFirmwareDlcNeedUpdateError;
        break;
      case guest_os::GuestOsDlcInstallation::Error::NeedReboot:
        result = BruschettaInstallResult::kFirmwareDlcNeedRebootError;
        break;
      case guest_os::GuestOsDlcInstallation::Error::DiskFull:
        result = BruschettaInstallResult::kFirmwareDlcDiskFullError;
        break;
      case guest_os::GuestOsDlcInstallation::Error::Busy:
        result = BruschettaInstallResult::kFirmwareDlcBusyError;
        break;
      case guest_os::GuestOsDlcInstallation::Error::Internal:
      case guest_os::GuestOsDlcInstallation::Error::Invalid:
      case guest_os::GuestOsDlcInstallation::Error::UnknownFailure:
      case guest_os::GuestOsDlcInstallation::Error::Cancelled:
      default:
        result = BruschettaInstallResult::kFirmwareDlcUnknownError;
        break;
    }
    Error(result);
    LOG(ERROR) << "Failed to install firmware dlc: " << install_result.error();
    return;
  }

  DownloadBootDisk();
}

void BruschettaInstallerImpl::DownloadBootDisk() {
  VLOG(2) << "Downloading boot disk";
  NotifyObserver(State::kBootDiskDownload);

  const std::string* url = config_.FindDict(prefs::kPolicyImageKey)
                               ->FindString(prefs::kPolicyURLKey);
  boot_disk_download_ = download_factory_.Run();
  boot_disk_download_->StartDownload(
      profile_, GURL(*url),
      base::BindOnce(&bruschetta::BruschettaInstallerImpl::OnBootDiskDownloaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstallerImpl::OnBootDiskDownloaded(base::FilePath path,
                                                   std::string hash) {
  if (MaybeClose()) {
    return;
  }
  if (path.empty()) {
    install_running_ = false;
    Error(BruschettaInstallResult::kDownloadError);
    return;
  }
  const std::string* expected = config_.FindDict(prefs::kPolicyImageKey)
                                    ->FindString(prefs::kPolicyHashKey);

  if (!base::EqualsCaseInsensitiveASCII(hash, *expected)) {
    install_running_ = false;
    Error(BruschettaInstallResult::kInvalidBootDisk);
    LOG(ERROR) << "Downloaded boot disk has incorrect hash";
    LOG(ERROR) << "Actual   " << hash;
    LOG(ERROR) << "Expected " << expected;
    return;
  }

  boot_disk_path_ = path;

  DownloadPflash();
}

void BruschettaInstallerImpl::DownloadPflash() {
  VLOG(2) << "Downloading pflash";
  NotifyObserver(State::kPflashDownload);
  const base::Value::Dict* pflash = config_.FindDict(prefs::kPolicyPflashKey);
  if (!pflash) {
    VLOG(2) << "No pflash file set, skipping to OpenFds";

    OpenFds();
    return;
  }

  const std::string* url = pflash->FindString(prefs::kPolicyURLKey);
  pflash_download_ = download_factory_.Run();
  pflash_download_->StartDownload(
      profile_, GURL(*url),
      base::BindOnce(&bruschetta::BruschettaInstallerImpl::OnPflashDownloaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstallerImpl::OnPflashDownloaded(base::FilePath path,
                                                 std::string hash) {
  if (MaybeClose()) {
    return;
  }
  if (path.empty()) {
    install_running_ = false;
    Error(BruschettaInstallResult::kDownloadError);
    return;
  }
  const std::string* expected = config_.FindDict(prefs::kPolicyPflashKey)
                                    ->FindString(prefs::kPolicyHashKey);

  if (!base::EqualsCaseInsensitiveASCII(hash, *expected)) {
    install_running_ = false;
    Error(BruschettaInstallResult::kInvalidPflash);
    LOG(ERROR) << "Downloaded pflash has incorrect hash";
    LOG(ERROR) << "Actual   " << hash;
    LOG(ERROR) << "Expected " << expected;
    return;
  }

  pflash_path_ = path;

  OpenFds();
}

void BruschettaInstallerImpl::OpenFds() {
  VLOG(2) << "Opening fds";
  NotifyObserver(State::kOpenFiles);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&OpenFdsBlocking, boot_disk_path_, pflash_path_,
                     profile_->GetPath()),
      base::BindOnce(&BruschettaInstallerImpl::OnOpenFds,
                     weak_ptr_factory_.GetWeakPtr()));
}

namespace {

std::unique_ptr<BruschettaInstallerImpl::Fds> OpenFdsBlocking(
    base::FilePath boot_disk_path,
    base::FilePath pflash_path,
    base::FilePath profile_path) {
  base::File boot_disk(boot_disk_path,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!boot_disk.IsValid()) {
    PLOG(ERROR) << "Failed to open boot disk";
    return nullptr;
  }

  std::optional<base::ScopedFD> pflash_fd;
  if (pflash_path.empty()) {
    pflash_fd = std::nullopt;
  } else {
    base::File pflash(pflash_path,
                      base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!pflash.IsValid()) {
      PLOG(ERROR) << "Failed to open pflash";
      return nullptr;
    }
    pflash_fd = base::ScopedFD(pflash.TakePlatformFile());
  }

  BruschettaInstallerImpl::Fds fds{
      .boot_disk = base::ScopedFD(boot_disk.TakePlatformFile()),
      .pflash = std::move(pflash_fd),
  };
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

  EnsureConciergeAvailable();
}

void BruschettaInstallerImpl::EnsureConciergeAvailable() {
  auto* client = ash::ConciergeClient::Get();
  DCHECK(client) << "This code requires a ConciergeClient";

  client->WaitForServiceToBeAvailable(
      base::BindOnce(&BruschettaInstallerImpl::OnConciergeAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstallerImpl::OnConciergeAvailable(bool service_is_available) {
  if (!service_is_available) {
    install_running_ = false;
    Error(BruschettaInstallResult::kConciergeUnavailableError);
    LOG(ERROR) << "vm_concierge is not available";
    return;
  }

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
  request.set_vm_name(vm_name_);
  request.set_image_type(vm_tools::concierge::DiskImageType::DISK_IMAGE_AUTO);
  request.set_storage_ballooning(true);

  client->CreateDiskImage(
      request, base::BindOnce(&BruschettaInstallerImpl::OnCreateVmDisk,
                              weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstallerImpl::OnCreateVmDisk(
    std::optional<vm_tools::concierge::CreateDiskImageResponse> result) {
  if (MaybeClose()) {
    return;
  }

  if (!result ||
      result->status() !=
          vm_tools::concierge::DiskImageStatus::DISK_STATUS_CREATED) {
    install_running_ = false;
    Error(BruschettaInstallResult::kCreateDiskError);
    if (result) {
      LOG(ERROR) << "Create VM disk failed: " << result->failure_reason();
    } else {
      LOG(ERROR) << "Create VM disk failed, no response";
    }
    return;
  }

  disk_path_ = result->disk_path();

  InstallPflash();
}

void BruschettaInstallerImpl::InstallPflash() {
  VLOG(2) << "Installing pflash file for VM";
  NotifyObserver(State::kInstallPflash);

  if (!fds_->pflash.has_value()) {
    VLOG(2) << "No pflash file expected, skipping to StartVm";
    ClearVek();
    return;
  }

  auto* client = ash::ConciergeClient::Get();
  DCHECK(client) << "This code requires a ConciergeClient";

  std::string user_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_);

  vm_tools::concierge::InstallPflashRequest request;

  request.set_owner_id(std::move(user_hash));
  request.set_vm_name(vm_name_);

  client->InstallPflash(
      std::move(*fds_->pflash), request,
      base::BindOnce(&BruschettaInstallerImpl::OnInstallPflash,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstallerImpl::OnInstallPflash(
    std::optional<vm_tools::concierge::InstallPflashResponse> result) {
  if (MaybeClose()) {
    return;
  }

  if (!result || !result->success()) {
    install_running_ = false;
    Error(BruschettaInstallResult::kInstallPflashError);
    if (result) {
      LOG(ERROR) << "Install pflash failed: " << result->failure_reason();
    } else {
      LOG(ERROR) << "Install pflash failed, no response";
    }
    return;
  }

  ClearVek();
}

void BruschettaInstallerImpl::ClearVek() {
  VLOG(2) << "Clearing VEK";
  NotifyObserver(State::kClearVek);

  attestation::DeleteKeysRequest request;
  request.set_username("");
  request.set_key_label_match(kVtpmEkLabel);
  request.set_match_behavior(
      attestation::DeleteKeysRequest::MATCH_BEHAVIOR_EXACT);

  auto* client = ash::AttestationClient::Get();
  DCHECK(client) << "This code requires a AttestationClient";

  client->DeleteKeys(request,
                     base::BindOnce(&BruschettaInstallerImpl::OnClearVek,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void BruschettaInstallerImpl::OnClearVek(
    const attestation::DeleteKeysReply& result) {
  if (MaybeClose()) {
    return;
  }

  if (result.status() != attestation::STATUS_SUCCESS) {
    install_running_ = false;
    Error(BruschettaInstallResult::kClearVekFailed);
    LOG(ERROR) << "Delete vEK failed: " << result.status();
    return;
  }

  StartVm();
}

void BruschettaInstallerImpl::StartVm() {
  VLOG(2) << "Starting VM";
  NotifyObserver(State::kStartVm);

  auto launch_policy_opt = GetLaunchPolicyForConfig(profile_, config_id_);
  auto full_policy_opt = GetInstallableConfig(profile_, config_id_);

  if (!full_policy_opt.has_value() || !launch_policy_opt.has_value()) {
    // Policy has changed to prohibit installation, so bail out before actually
    // starting the VM.
    install_running_ = false;
    Error(BruschettaInstallResult::kInstallationProhibited);
    LOG(ERROR) << "Installation prohibited by policy";
    return;
  }
  auto launch_policy = *launch_policy_opt;
  const auto* full_policy = *full_policy_opt;

  auto* client = ash::ConciergeClient::Get();
  DCHECK(client) << "This code requires a ConciergeClient";

  std::string user_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_);
  std::string vm_username = GetVmUsername(profile_);
  vm_tools::concierge::StartVmRequest request;

  request.set_name(vm_name_);
  request.set_owner_id(std::move(user_hash));
  request.set_vm_username(vm_username);
  request.mutable_vm()->set_tools_dlc_id(kToolsDlc);
  request.mutable_vm()->set_bios_dlc_id(kUefiDlc);
  request.set_start_termina(false);
  request.set_vtpm_proxy(launch_policy.vtpm_enabled);

  auto* disk = request.add_disks();
  disk->set_path(std::move(disk_path_));
  disk->set_writable(true);

  for (const auto& oem_string :
       *full_policy->FindList(prefs::kPolicyOEMStringsKey)) {
    request.add_oem_strings(oem_string.GetString());
  }

  request.set_timeout(240);

  request.add_fds(vm_tools::concierge::StartVmRequest::STORAGE);

  client->StartVmWithFd(
      std::move(fds_->boot_disk), request,
      base::BindOnce(&BruschettaInstallerImpl::OnStartVm,
                     weak_ptr_factory_.GetWeakPtr(), launch_policy));

  fds_.reset();
}

void BruschettaInstallerImpl::OnStartVm(
    RunningVmPolicy launch_policy,
    std::optional<vm_tools::concierge::StartVmResponse> result) {
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

  BruschettaServiceFactory::GetForProfile(profile_)->RegisterVmLaunch(
      vm_name_, launch_policy);
  profile_->GetPrefs()->SetBoolean(bruschetta::prefs::kBruschettaInstalled,
                                   true);

  LaunchTerminal();
}

void BruschettaInstallerImpl::LaunchTerminal() {
  VLOG(2) << "Launching terminal";
  NotifyObserver(State::kLaunchTerminal);

  // TODO(b/231899688): Implement Bruschetta sending an RPC when installation
  // finishes so that we only add to prefs on success.
  auto guest_id = MakeBruschettaId(std::move(vm_name_));
  BruschettaServiceFactory::GetForProfile(profile_)->RegisterInPrefs(
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
