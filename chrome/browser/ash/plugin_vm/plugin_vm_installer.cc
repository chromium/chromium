// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_installer.h"

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_drive_image_download_service.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_license_checker.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/network_service_instance.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

// This file contains VLOG logging to aid debugging tast tests.
#define LOG_FUNCTION_CALL() \
  VLOG(2) << "PluginVmInstaller::" << __func__ << " called"

namespace plugin_vm {

namespace {

constexpr int64_t kBytesPerGigabyte = 1024 * 1024 * 1024;
// Size to use for calculating progress when the actual size isn't available.
constexpr int64_t kDownloadSizeFallbackEstimate = 15LL * kBytesPerGigabyte;

constexpr char kFailureReasonHistogram[] = "PluginVm.SetupFailureReason";

constexpr char kHomeDirectory[] = "/home/chronos/user";

ash::ConciergeClient* GetConciergeClient() {
  return ash::ConciergeClient::Get();
}

constexpr char kIsoSignature[] = "CD001";
constexpr int64_t kIsoOffsets[] = {0x8001, 0x8801, 0x9001};

bool IsIsoImage(const base::FilePath& image) {
  base::File file(image, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open " << image.value();
    return false;
  }

  std::vector<uint8_t> data(strlen(kIsoSignature));
  for (auto offset : kIsoOffsets) {
    if (file.ReadAndCheck(offset, data) &&
        std::string(data.begin(), data.end()) == kIsoSignature) {
      return true;
    }
  }
  return false;
}

std::optional<base::ScopedFD> PrepareFD(const base::FilePath& image) {
  base::File file(image, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open " << image.value();
    return std::nullopt;
  }

  return base::ScopedFD(file.TakePlatformFile());
}

PluginVmSetupResult BucketForCancelledInstall(
    PluginVmInstaller::InstallingState installing_state) {
  switch (installing_state) {
    case PluginVmInstaller::InstallingState::kInactive:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case PluginVmInstaller::InstallingState::kCheckingLicense:
      return PluginVmSetupResult::kUserCancelledValidatingLicense;
    case PluginVmInstaller::InstallingState::kCheckingDiskSpace:
      return PluginVmSetupResult::kUserCancelledCheckingDiskSpace;
    case PluginVmInstaller::InstallingState::kDownloadingDlc:
      return PluginVmSetupResult::kUserCancelledDownloadingPluginVmDlc;
    case PluginVmInstaller::InstallingState::kStartingDispatcher:
      return PluginVmSetupResult::kUserCancelledStartingDispatcher;
    case PluginVmInstaller::InstallingState::kCheckingForExistingVm:
      return PluginVmSetupResult::kUserCancelledCheckingForExistingVm;
    case PluginVmInstaller::InstallingState::kDownloadingImage:
      return PluginVmSetupResult::kUserCancelledDownloadingPluginVmImage;
    case PluginVmInstaller::InstallingState::kImporting:
      return PluginVmSetupResult::kUserCancelledImportingPluginVmImage;
  }
}

}  // namespace

PluginVmInstaller::PluginVmInstaller(Profile* profile)
    : profile_(profile),
      download_service_(BackgroundDownloadServiceFactory::GetForKey(
          profile->GetProfileKey())) {}

std::optional<PluginVmInstaller::FailureReason> PluginVmInstaller::Start() {
  LOG_FUNCTION_CALL();
  if (IsProcessing()) {
    LOG(ERROR) << "Download of a PluginVm image couldn't be started as"
               << " another PluginVm image is currently being processed "
               << "in state " << GetStateName(state_) << ", "
               << GetInstallingStateName(installing_state_);
    return FailureReason::OPERATION_IN_PROGRESS;
  }

  // Defensive check preventing any download attempts when PluginVm is
  // not allowed to run (this might happen in rare cases if PluginVm has
  // been disabled but the installer icon is still visible).
  if (!PluginVmFeatures::Get()->IsAllowed(profile_)) {
    LOG(ERROR) << "Download of PluginVm image cannot be started because "
               << "the user is not allowed to run PluginVm";
    return FailureReason::NOT_ALLOWED;
  }

  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    return FailureReason::OFFLINE;
  }

  // Reset camera/mic permissions, we don't want it to persist across
  // re-installation.
  profile_->GetPrefs()->SetBoolean(prefs::kPluginVmCameraAllowed, false);
  profile_->GetPrefs()->SetBoolean(prefs::kPluginVmMicAllowed, false);

  // Request wake lock when state_ goes to kInstalling, and cancel it when state
  // goes back to kIdle.
  GetWakeLock()->RequestWakeLock();
  state_ = State::kInstalling;
  progress_ = 0;

  // Perform the first step asynchronously to ensure OnError() isn't called
  // before Start() returns.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PluginVmInstaller::CheckLicense,
                                weak_ptr_factory_.GetWeakPtr()));

  return std::nullopt;
}

void PluginVmInstaller::Cancel() {
  LOG_FUNCTION_CALL();
  if (state_ != State::kInstalling) {
    RecordPluginVmSetupResultHistogram(
        PluginVmSetupResult::kUserCancelledWithoutStarting);
    return;
  }

  RecordPluginVmSetupResultHistogram(
      BucketForCancelledInstall(installing_state_));

  state_ = State::kCancelling;
  switch (installing_state_) {
    case InstallingState::kCheckingLicense:
    case InstallingState::kCheckingDiskSpace:
    case InstallingState::kCheckingForExistingVm:
    case InstallingState::kStartingDispatcher:
      // These can't be cancelled, so we wait for completion.
      return;
    case InstallingState::kDownloadingDlc:
      //  For DLC, we also block progress callbacks.
      dlc_installation_.reset();
      return;
    case InstallingState::kDownloadingImage:
      CancelDownload();
      return;
    case InstallingState::kImporting:
      CancelImport();
      return;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

bool PluginVmInstaller::IsProcessing() {
  return state_ != State::kIdle;
}

void PluginVmInstaller::SetObserver(Observer* observer) {
  observer_ = observer;
}

void PluginVmInstaller::RemoveObserver() {
  observer_ = nullptr;
}

std::string PluginVmInstaller::GetCurrentDownloadGuid() {
  return current_download_guid_;
}

void PluginVmInstaller::OnDownloadStarted() {}

void PluginVmInstaller::OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                                  int64_t content_length) {
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingImage);

  if (expected_image_size_ == kImageSizeUnknown) {
    if (content_length > 0) {
      expected_image_size_ = content_length;
    }
  } else if (expected_image_size_ > 0) {
    if (content_length != expected_image_size_) {
      expected_image_size_ = kImageSizeError;
    }
  }

  if (observer_) {
    observer_->OnDownloadProgressUpdated(bytes_downloaded, content_length);
  }

  if (content_length <= 0) {
    content_length = kDownloadSizeFallbackEstimate;
  }

  UpdateProgress(
      std::min(1., static_cast<double>(bytes_downloaded) / content_length));
}

void PluginVmInstaller::OnDownloadCompleted(
    const download::CompletionInfo& info) {
  downloaded_image_ = info.path;
  downloaded_image_size_ = info.bytes_downloaded;
  current_download_guid_.clear();

  if (downloaded_image_for_testing_) {
    downloaded_image_ = downloaded_image_for_testing_.value();
  }

  if (!VerifyDownload(info.hash256)) {
    LOG(ERROR) << "Expected image size: " << expected_image_size_
               << ", downloaded image size: " << downloaded_image_size_;
    if (expected_image_size_ == kImageSizeUnknown ||
        expected_image_size_ == downloaded_image_size_) {
      OnDownloadFailed(FailureReason::HASH_MISMATCH);
    } else {
      OnDownloadFailed(FailureReason::DOWNLOAD_SIZE_MISMATCH);
    }
    return;
  }

  StartImport();
}

void PluginVmInstaller::OnDownloadFailed(FailureReason reason) {
  RemoveTemporaryImageIfExists();
  current_download_guid_.clear();

  if (using_drive_download_service_) {
    drive_download_service_->ResetState();
    using_drive_download_service_ = false;
  }

  InstallFailed(reason);
}

void PluginVmInstaller::OnDiskImageProgress(
    const vm_tools::concierge::DiskImageStatusResponse& signal) {
  if (signal.command_uuid() != current_import_command_uuid_) {
    return;
  }

  const uint64_t percent_completed = signal.progress();
  const vm_tools::concierge::DiskImageStatus status = signal.status();

  switch (status) {
    case vm_tools::concierge::DiskImageStatus::DISK_STATUS_CREATED:
      VLOG(1) << "Disk image status indicates that importing is done.";
      RequestFinalStatus();
      return;
    case vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS:
      UpdateProgress(percent_completed / 100.);
      return;
    case vm_tools::concierge::DiskImageStatus::DISK_STATUS_NOT_ENOUGH_SPACE:
      LOG(ERROR) << "Disk image import signals out of space condition with "
                    "current progress: "
                 << percent_completed;
      OnImported(FailureReason::OUT_OF_DISK_SPACE);
      return;
    default:
      LOG(ERROR) << "Disk image status signal has status: " << status
                 << " with error message: " << signal.failure_reason()
                 << " and current progress: " << percent_completed;
      OnImported(FailureReason::UNEXPECTED_DISK_IMAGE_STATUS);
      return;
  }
}

bool PluginVmInstaller::VerifyDownload(
    const std::string& downloaded_archive_hash) {
  if (downloaded_archive_hash.empty()) {
    LOG(ERROR) << "No hash found for downloaded PluginVm image archive";
    return false;
  }
  const base::Value* plugin_vm_image_hash_ptr =
      profile_->GetPrefs()
          ->GetDict(prefs::kPluginVmImage)
          .Find(prefs::kPluginVmImageHashKeyName);
  if (!plugin_vm_image_hash_ptr) {
    LOG(ERROR) << "Hash of PluginVm image is not specified";
    return false;
  }
  std::string plugin_vm_image_hash = plugin_vm_image_hash_ptr->GetString();

  if (!base::EqualsCaseInsensitiveASCII(plugin_vm_image_hash,
                                        downloaded_archive_hash)) {
    LOG(ERROR) << "Downloaded PluginVm image archive hash ("
               << downloaded_archive_hash << ") doesn't match "
               << "hash specified by the PluginVmImage policy ("
               << plugin_vm_image_hash << ")";
    return false;
  }

  return true;
}

int64_t PluginVmInstaller::RequiredFreeDiskSpace() {
  return static_cast<int64_t>(profile_->GetPrefs()->GetInteger(
             prefs::kPluginVmRequiredFreeDiskSpaceGB)) *
         kBytesPerGigabyte;
}

void PluginVmInstaller::SetDownloadServiceForTesting(
    download::BackgroundDownloadService* download_service) {
  download_service_ = download_service;
}

void PluginVmInstaller::SetDownloadedImageForTesting(
    const base::FilePath& downloaded_image) {
  downloaded_image_for_testing_ = downloaded_image;
}

void PluginVmInstaller::SetDriveDownloadServiceForTesting(
    std::unique_ptr<PluginVmDriveImageDownloadService> drive_download_service) {
  drive_download_service_ = std::move(drive_download_service);
}

PluginVmInstaller::~PluginVmInstaller() = default;

void PluginVmInstaller::CheckLicense() {
  UpdateInstallingState(InstallingState::kCheckingLicense);

  if (skip_license_check_for_testing_) {
    OnLicenseChecked(true);
    return;
  }
  license_checker_ = std::make_unique<PluginVmLicenseChecker>(profile_);
  license_checker_->CheckLicense(base::BindOnce(
      &PluginVmInstaller::OnLicenseChecked, weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnLicenseChecked(bool license_is_valid) {
  if (state_ == State::kCancelling) {
    CancelFinished();
    return;
  }

  if (!license_is_valid) {
    LOG(ERROR) << "Install of a PluginVm image couldn't be started as"
               << " there is not a valid license associated with the user.";
    InstallFailed(FailureReason::INVALID_LICENSE);
    return;
  }

  CheckForExistingVm();
}

void PluginVmInstaller::CheckForExistingVm() {
  DCHECK_EQ(installing_state_, InstallingState::kCheckingLicense);
  UpdateInstallingState(InstallingState::kCheckingForExistingVm);

  GetConciergeClient()->WaitForServiceToBeAvailable(
      base::BindOnce(&PluginVmInstaller::OnConciergeAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnConciergeAvailable(bool success) {
  if (!success) {
    LOG(ERROR) << "Concierge did not become available";
    OnImported(FailureReason::CONCIERGE_NOT_AVAILABLE);
    return;
  }

  vm_tools::concierge::ListVmDisksRequest request;
  request.set_cryptohome_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
  request.set_all_locations(true);
  request.set_vm_name(kPluginVmName);

  GetConciergeClient()->ListVmDisks(
      std::move(request), base::BindOnce(&PluginVmInstaller::OnListVmDisks,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnListVmDisks(
    std::optional<vm_tools::concierge::ListVmDisksResponse> response) {
  if (state_ == State::kCancelling) {
    CancelFinished();
    return;
  }

  if (!response || !response->success()) {
    LOG(ERROR) << "Failed to list VM disks: "
               << (response ? response->failure_reason() : "[Empty response]");
    InstallFailed(FailureReason::LIST_VM_DISKS_FAILED);
    return;
  }

  if (response->images_size() > 0) {
    auto& image = response->images(0);
    if (image.storage_location() ==
        vm_tools::concierge::STORAGE_CRYPTOHOME_PLUGINVM) {
      RecordPluginVmSetupResultHistogram(PluginVmSetupResult::kVmAlreadyExists);
      if (observer_) {
        observer_->OnVmExists();
      }
      profile_->GetPrefs()->SetBoolean(prefs::kPluginVmImageExists, true);
      InstallFinished();
    } else {
      LOG(ERROR) << "VM " << image.name() << " exists, but in wrong location";
      InstallFailed(FailureReason::EXISTING_IMAGE_INVALID);
    }
    return;
  }

  CheckDiskSpace();
}

void PluginVmInstaller::CheckDiskSpace() {
  DCHECK_EQ(installing_state_, InstallingState::kCheckingForExistingVm);
  UpdateInstallingState(InstallingState::kCheckingDiskSpace);

  ash::SpacedClient::Get()->GetFreeDiskSpace(
      kHomeDirectory, base::BindOnce(&PluginVmInstaller::OnAvailableDiskSpace,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnAvailableDiskSpace(std::optional<int64_t> bytes) {
  if (state_ == State::kCancelling) {
    CancelFinished();
    return;
  }

  if (free_disk_space_for_testing_ != -1) {
    bytes = std::optional<int64_t>(free_disk_space_for_testing_);
  }

  if (!bytes.has_value() || bytes.value() < RequiredFreeDiskSpace()) {
    InstallFailed(FailureReason::INSUFFICIENT_DISK_SPACE);
    return;
  }

  StartDlcDownload();
}

void PluginVmInstaller::StartDlcDownload() {
  LOG_FUNCTION_CALL();
  DCHECK_EQ(installing_state_, InstallingState::kCheckingDiskSpace);
  UpdateInstallingState(InstallingState::kDownloadingDlc);

  if (!GetPluginVmImageDownloadUrl().is_valid()) {
    InstallFailed(FailureReason::INVALID_IMAGE_URL);
    return;
  }

  dlc_installation_ = std::make_unique<guest_os::GuestOsDlcInstallation>(
      kPitaDlc,
      base::BindOnce(&PluginVmInstaller::OnDlcDownloadCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PluginVmInstaller::OnDlcDownloadProgressUpdated,
                          weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnDlcDownloadProgressUpdated(double progress) {
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingDlc);
  if (state_ == State::kCancelling) {
    return;
  }

  UpdateProgress(progress);
}

void PluginVmInstaller::OnDlcDownloadCompleted(
    guest_os::GuestOsDlcInstallation::Result install_result) {
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingDlc);
  dlc_installation_.reset();

  // If success, continue to the next state.
  if (install_result.has_value()) {
    RecordPluginVmDlcUseResultHistogram(PluginVmDlcUseResult::kDlcSuccess);
    StartDispatcher();
    return;
  }

  // At this point, PluginVM DLC download failed.
  PluginVmDlcUseResult result = PluginVmDlcUseResult::kInternalDlcError;
  FailureReason reason = FailureReason::DLC_INTERNAL;

  switch (install_result.error()) {
    case guest_os::GuestOsDlcInstallation::Error::Cancelled:
      DCHECK(state_ == State::kCancelling);
      CancelFinished();
      return;
    case guest_os::GuestOsDlcInstallation::Error::Invalid:
      LOG(ERROR)
          << "PluginVM DLC is not supported, need to enable PluginVM DLC.";
      result = PluginVmDlcUseResult::kInvalidDlcError;
      reason = FailureReason::DLC_UNSUPPORTED;
      break;
    case guest_os::GuestOsDlcInstallation::Error::Busy:
      LOG(ERROR)
          << "PluginVM DLC is not able to be downloaded as dlcservice is busy.";
      result = PluginVmDlcUseResult::kBusyDlcError;
      reason = FailureReason::DLC_BUSY;
      break;
    case guest_os::GuestOsDlcInstallation::Error::NeedReboot:
      LOG(ERROR) << "Device has pending update and needs a reboot to use "
                    "PluginVM DLC.";
      result = PluginVmDlcUseResult::kNeedRebootDlcError;
      reason = FailureReason::DLC_NEED_REBOOT;
      break;
    case guest_os::GuestOsDlcInstallation::Error::DiskFull:
      LOG(ERROR) << "Device needs to free space to use PluginVM DLC.";
      result = PluginVmDlcUseResult::kNeedSpaceDlcError;
      reason = FailureReason::DLC_NEED_SPACE;
      break;
    case guest_os::GuestOsDlcInstallation::Error::NeedUpdate:
      LOG(ERROR) << "The PluginVM DLC could not be found in the server."
                 << "The version the OS is on is probably not live.";
      result = PluginVmDlcUseResult::kNoImageFoundDlcError;
      // Keep using the reason `FailureReason::DLC_INTERNAL`, but distinguish so
      // developers can see why it wasn't updated as well as for metrics
      // reporting.
      break;
    case guest_os::GuestOsDlcInstallation::Error::Offline:
    case guest_os::GuestOsDlcInstallation::Error::Internal:
    case guest_os::GuestOsDlcInstallation::Error::UnknownFailure:
      LOG(ERROR) << "Failed to download PluginVM DLC: "
                 << install_result.error();
      break;
  }

  RecordPluginVmDlcUseResultHistogram(result);
  InstallFailed(reason);
}

void PluginVmInstaller::StartDispatcher() {
  LOG_FUNCTION_CALL();
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingDlc);
  UpdateInstallingState(InstallingState::kStartingDispatcher);

  PluginVmManagerFactory::GetForProfile(profile_)->StartDispatcher(
      base::BindOnce(&PluginVmInstaller::OnDispatcherStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnDispatcherStarted(bool success) {
  if (state_ == State::kCancelling) {
    CancelFinished();
    return;
  }

  if (!success) {
    InstallFailed(FailureReason::DISPATCHER_NOT_AVAILABLE);
    return;
  }

  StartDownload();
}

void PluginVmInstaller::StartDownload() {
  DCHECK_EQ(installing_state_, InstallingState::kStartingDispatcher);
  UpdateInstallingState(InstallingState::kDownloadingImage);
  UpdateProgress(/*state_progress=*/0);

  GURL url = GetPluginVmImageDownloadUrl();
  // This may have changed since running StartDlcDownload.
  if (!url.is_valid()) {
    InstallFailed(FailureReason::INVALID_IMAGE_URL);
    return;
  }

  expected_image_size_ = kImageSizeUnknown;
  downloaded_image_size_ = kImageSizeUnknown;
  std::optional<std::string> drive_id = GetIdFromDriveUrl(url);
  using_drive_download_service_ = drive_id.has_value();

  if (using_drive_download_service_) {
    if (!drive_download_service_) {
      drive_download_service_ =
          std::make_unique<PluginVmDriveImageDownloadService>(this, profile_);
    } else {
      drive_download_service_->ResetState();
    }

    drive_download_service_->StartDownload(drive_id.value());
  } else {
    download_service_->StartDownload(GetDownloadParams(url));
  }
}

void PluginVmInstaller::OnStartDownload(
    const std::string& download_guid,
    download::DownloadParams::StartResult start_result) {
  if (start_result == download::DownloadParams::ACCEPTED) {
    current_download_guid_ = download_guid;
  } else {
    OnDownloadFailed(FailureReason::DOWNLOAD_FAILED_UNKNOWN);
  }
}

void PluginVmInstaller::StartImport() {
  LOG_FUNCTION_CALL();
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingImage);
  UpdateInstallingState(InstallingState::kImporting);
  UpdateProgress(/*state_progress=*/0);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&IsIsoImage, downloaded_image_),
      base::BindOnce(&PluginVmInstaller::OnImageTypeDetected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnImageTypeDetected(bool is_iso_image) {
  creating_new_vm_ = is_iso_image;

  if (!GetConciergeClient()->IsDiskImageProgressSignalConnected()) {
    LOG(ERROR) << "Disk image progress signal is not connected";
    OnImported(FailureReason::SIGNAL_NOT_CONNECTED);
    return;
  }

  GetConciergeClient()->AddDiskImageObserver(this);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&PrepareFD, downloaded_image_),
      base::BindOnce(&PluginVmInstaller::OnFDPrepared,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnFDPrepared(std::optional<base::ScopedFD> maybeFd) {
  // In case import has been cancelled meantime.
  if (state_ != State::kInstalling) {
    return;
  }

  if (!maybeFd.has_value()) {
    LOG(ERROR) << "Could not open downloaded image";
    OnImported(FailureReason::COULD_NOT_OPEN_IMAGE);
    return;
  }

  base::ScopedFD fd(std::move(maybeFd.value()));

  if (creating_new_vm_) {
    vm_tools::concierge::CreateDiskImageRequest request;
    request.set_cryptohome_id(
        ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
    request.set_vm_name(kPluginVmName);
    request.set_storage_location(
        vm_tools::concierge::STORAGE_CRYPTOHOME_PLUGINVM);
    request.set_source_size(downloaded_image_size_);

    VLOG(1) << "Making call to concierge to set up VM from an ISO";

    GetConciergeClient()->CreateDiskImageWithFd(
        std::move(fd), request,
        base::BindOnce(&PluginVmInstaller::OnImportDiskImage<
                           vm_tools::concierge::CreateDiskImageResponse>,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    vm_tools::concierge::ImportDiskImageRequest request;
    request.set_cryptohome_id(
        ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
    request.set_vm_name(kPluginVmName);
    request.set_storage_location(
        vm_tools::concierge::STORAGE_CRYPTOHOME_PLUGINVM);
    request.set_source_size(downloaded_image_size_);

    VLOG(1) << "Making call to concierge to import disk image";

    GetConciergeClient()->ImportDiskImage(
        std::move(fd), request,
        base::BindOnce(&PluginVmInstaller::OnImportDiskImage<
                           vm_tools::concierge::ImportDiskImageResponse>,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

template <typename ReplyType>
void PluginVmInstaller::OnImportDiskImage(std::optional<ReplyType> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Could not retrieve response from Create/ImportDiskImage "
               << "call to concierge";
    OnImported(FailureReason::INVALID_IMPORT_RESPONSE);
    return;
  }

  ReplyType response = reply.value();

  switch (response.status()) {
    case vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS:
      VLOG(1) << "Disk image creation/import is now in progress";
      current_import_command_uuid_ = response.command_uuid();
      // Image in progress. Waiting for progress signals...
      // TODO(crbug.com/41460680): think about adding a timeout here,
      //   i.e. what happens if concierge dies and does not report any signal
      //   back, not even an error signal. Right now, the user would see
      //   the "Configuring Plugin VM" screen forever. Maybe that's OK
      //   at this stage though.
      break;
    case vm_tools::concierge::DiskImageStatus::DISK_STATUS_NOT_ENOUGH_SPACE:
      LOG(ERROR) << "Disk image import operation ran out of disk space";
      OnImported(FailureReason::OUT_OF_DISK_SPACE);
      break;
    default:
      LOG(ERROR) << "Disk image is not in progress. Status: "
                 << response.status() << ", " << response.failure_reason();
      OnImported(FailureReason::UNEXPECTED_DISK_IMAGE_STATUS);
      break;
  }
}

void PluginVmInstaller::RequestFinalStatus() {
  vm_tools::concierge::DiskImageStatusRequest status_request;
  status_request.set_command_uuid(current_import_command_uuid_);
  GetConciergeClient()->DiskImageStatus(
      status_request, base::BindOnce(&PluginVmInstaller::OnFinalDiskImageStatus,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnFinalDiskImageStatus(
    std::optional<vm_tools::concierge::DiskImageStatusResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Could not retrieve response from DiskImageStatus call to "
               << "concierge";
    OnImported(FailureReason::INVALID_DISK_IMAGE_STATUS_RESPONSE);
    return;
  }

  vm_tools::concierge::DiskImageStatusResponse response = reply.value();
  DCHECK(response.command_uuid() == current_import_command_uuid_);
  switch (response.status()) {
    case vm_tools::concierge::DiskImageStatus::DISK_STATUS_CREATED:
      OnImported(std::nullopt);
      break;
    case vm_tools::concierge::DiskImageStatus::DISK_STATUS_NOT_ENOUGH_SPACE:
      LOG(ERROR) << "Disk image import operation ran out of disk space "
                 << "with current progress: " << response.progress();
      OnImported(FailureReason::OUT_OF_DISK_SPACE);
      break;
    default:
      LOG(ERROR) << "Disk image is not created. Status: " << response.status()
                 << ", " << response.failure_reason();
      OnImported(FailureReason::IMAGE_IMPORT_FAILED);
      break;
  }
}

void PluginVmInstaller::OnImported(
    std::optional<FailureReason> failure_reason) {
  LOG_FUNCTION_CALL();
  GetConciergeClient()->RemoveDiskImageObserver(this);
  RemoveTemporaryImageIfExists();
  current_import_command_uuid_.clear();

  if (failure_reason) {
    if (creating_new_vm_) {
      LOG(ERROR) << "New VM creation failed";
    } else {
      LOG(ERROR) << "Image import failed";
    }
    InstallFailed(*failure_reason);
    return;
  }

  profile_->GetPrefs()->SetBoolean(prefs::kPluginVmImageExists, true);
  RecordPluginVmSetupResultHistogram(PluginVmSetupResult::kSuccess);
  if (observer_) {
    if (creating_new_vm_) {
      observer_->OnCreated();
    } else {
      observer_->OnImported();
    }
  }
  InstallFinished();
}

void PluginVmInstaller::UpdateInstallingState(
    InstallingState installing_state) {
  LOG_FUNCTION_CALL() << " with state "
                      << GetInstallingStateName(installing_state);
  DCHECK_NE(installing_state, InstallingState::kInactive);
  installing_state_ = installing_state;
  observer_->OnStateUpdated(installing_state_);
}

void PluginVmInstaller::UpdateProgress(double state_progress) {
  DCHECK_EQ(state_, State::kInstalling);
  if (state_progress < 0 || state_progress > 1) {
    LOG(ERROR) << "Unexpected progress value " << state_progress
               << " in installing state "
               << GetInstallingStateName(installing_state_);
    return;
  }

  double start_range = 0;
  double end_range = 0;
  switch (installing_state_) {
    case InstallingState::kDownloadingDlc:
      start_range = 0;
      end_range = 0.01;
      break;
    case InstallingState::kDownloadingImage:
      start_range = 0.01;
      end_range = 0.45;
      break;
    case InstallingState::kImporting:
      start_range = 0.45;
      end_range = 1;
      break;
    default:
      // Other states take a negligible amount of time so we don't send progress
      // updates.
      NOTREACHED_IN_MIGRATION();
  }

  double new_progress =
      start_range + (end_range - start_range) * state_progress;
  if (new_progress < progress_) {
    LOG(ERROR) << "Progress went backwards from " << progress_ << " to "
               << new_progress;
    return;
  }

  progress_ = new_progress;
  if (observer_) {
    observer_->OnProgressUpdated(new_progress);
  }
}

void PluginVmInstaller::InstallFailed(FailureReason reason) {
  LOG_FUNCTION_CALL() << " with failure reason " << static_cast<int>(reason);
  state_ = State::kIdle;
  GetWakeLock()->CancelWakeLock();
  installing_state_ = InstallingState::kInactive;
  base::UmaHistogramEnumeration(kFailureReasonHistogram, reason);
  RecordPluginVmSetupResultHistogram(PluginVmSetupResult::kError);
  if (observer_) {
    observer_->OnError(reason);
  }
}

void PluginVmInstaller::InstallFinished() {
  LOG_FUNCTION_CALL();
  state_ = State::kIdle;
  GetWakeLock()->CancelWakeLock();
  installing_state_ = InstallingState::kInactive;
}

void PluginVmInstaller::CancelDownload() {
  if (using_drive_download_service_) {
    DCHECK(drive_download_service_);
    drive_download_service_->CancelDownload();
  } else {
    download_service_->CancelDownload(current_download_guid_);
    current_download_guid_.clear();
  }
  CancelFinished();
}

void PluginVmInstaller::CancelImport() {
  VLOG(1) << "Cancelling disk image import with command_uuid: "
          << current_import_command_uuid_;

  vm_tools::concierge::CancelDiskImageRequest request;
  request.set_command_uuid(current_import_command_uuid_);
  GetConciergeClient()->CancelDiskImageOperation(
      request, base::BindOnce(&PluginVmInstaller::OnImportDiskImageCancelled,
                              weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnImportDiskImageCancelled(
    std::optional<vm_tools::concierge::CancelDiskImageResponse> reply) {
  DCHECK_EQ(state_, State::kCancelling);
  DCHECK_EQ(installing_state_, InstallingState::kImporting);

  RemoveTemporaryImageIfExists();

  if (!reply.has_value()) {
    LOG(ERROR) << "Could not retrieve response from CancelDiskImageOperation "
               << "call to concierge";
    CancelFinished();
    return;
  }

  vm_tools::concierge::CancelDiskImageResponse response = reply.value();
  if (response.success()) {
    VLOG(1) << "Import disk image request has been cancelled successfully";
  } else {
    LOG(ERROR) << "Import disk image request failed to be cancelled, "
               << response.failure_reason();
  }

  CancelFinished();
}

void PluginVmInstaller::CancelFinished() {
  DCHECK_EQ(state_, State::kCancelling);
  state_ = State::kIdle;
  GetWakeLock()->CancelWakeLock();
  installing_state_ = InstallingState::kInactive;

  if (observer_) {
    observer_->OnCancelFinished();
  }
}

std::string PluginVmInstaller::GetStateName(State state) {
  switch (state) {
    case State::kIdle:
      return "kIdle";
    case State::kInstalling:
      return "kInstalling";
    case State::kCancelling:
      return "kCancelling";
  }
}

std::string PluginVmInstaller::GetInstallingStateName(InstallingState state) {
  switch (state) {
    case InstallingState::kInactive:
      return "kInactive";
    case InstallingState::kCheckingDiskSpace:
      return "kCheckingDiskSpace";
    case InstallingState::kCheckingForExistingVm:
      return "kCheckingForExistingVm";
    case InstallingState::kDownloadingDlc:
      return "kDownloadingDlc";
    case InstallingState::kStartingDispatcher:
      return "kStartingDispatcher";
    case InstallingState::kDownloadingImage:
      return "kDownloadingImage";
    case InstallingState::kImporting:
      return "kImporting";
    case InstallingState::kCheckingLicense:
      return "kCheckingLicense";
  }
}

GURL PluginVmInstaller::GetPluginVmImageDownloadUrl() {
  const base::Value* url_ptr = profile_->GetPrefs()
                                   ->GetDict(prefs::kPluginVmImage)
                                   .Find(prefs::kPluginVmImageUrlKeyName);
  if (!url_ptr) {
    LOG(ERROR) << "Url to PluginVm image is not specified";
    return GURL();
  }
  return GURL(url_ptr->GetString());
}

download::DownloadParams PluginVmInstaller::GetDownloadParams(const GURL& url) {
  download::DownloadParams params;

  // DownloadParams
  params.client = download::DownloadClient::PLUGIN_VM_IMAGE;
  params.guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  params.callback = base::BindRepeating(&PluginVmInstaller::OnStartDownload,
                                        weak_ptr_factory_.GetWeakPtr());

  params.traffic_annotation = net::MutableNetworkTrafficAnnotationTag(
      kPluginVmNetworkTrafficAnnotation);

  // RequestParams
  params.request_params.url = url;
  params.request_params.method = "GET";
  // Disable Safe Browsing/checks because the download is system-initiated,
  // the target is specified via enterprise policy, and contents will be
  // validated by comparing hashes.
  params.request_params.require_safety_checks = false;

  // SchedulingParams
  // User initiates download by clicking on PluginVm icon so priorities should
  // be the highest.
  params.scheduling_params.priority = download::SchedulingParams::Priority::UI;
  params.scheduling_params.battery_requirements =
      download::SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
  params.scheduling_params.network_requirements =
      download::SchedulingParams::NetworkRequirements::NONE;

  return params;
}

void PluginVmInstaller::RemoveTemporaryImageIfExists() {
  if (using_drive_download_service_) {
    drive_download_service_->RemoveTemporaryArchive(
        base::BindOnce(&PluginVmInstaller::OnTemporaryImageRemoved,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (!downloaded_image_.empty()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&base::DeleteFile, downloaded_image_),
        base::BindOnce(&PluginVmInstaller::OnTemporaryImageRemoved,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void PluginVmInstaller::OnTemporaryImageRemoved(bool success) {
  if (!success) {
    LOG(ERROR) << "Downloaded PluginVm image located in "
               << downloaded_image_.value() << " failed to be deleted";
    return;
  }
  downloaded_image_.clear();
  creating_new_vm_ = false;
}

device::mojom::WakeLock* PluginVmInstaller::GetWakeLock() {
  if (!wake_lock_) {
    mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
    content::GetDeviceService().BindWakeLockProvider(
        wake_lock_provider.BindNewPipeAndPassReceiver());
    wake_lock_provider->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventAppSuspension,
        device::mojom::WakeLockReason::kOther, "Plugin VM Installer",
        wake_lock_.BindNewPipeAndPassReceiver());
  }
  return wake_lock_.get();
}

}  // namespace plugin_vm

#undef LOG_FUNCTION_CALL
