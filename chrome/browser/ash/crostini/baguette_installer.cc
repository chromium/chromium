// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/baguette_installer.h"

#include <algorithm>
#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/crostini/baguette_download.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/prefs/pref_service.h"
#include "url/origin.h"

namespace crostini {

namespace {

base::ScopedFD OpenFdBlocking(base::FilePath image_path) {
  base::File image(image_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!image.IsValid()) {
    LOG(ERROR) << "Failed to open image file";
    return base::ScopedFD();
  }
  return base::ScopedFD(image.TakePlatformFile());
}

}  // namespace

BaguetteInstaller::BaguetteInstaller(Profile* profile, PrefService& local_state)
    : download_factory_(base::BindRepeating(
          [](PrefService& local_state) -> std::unique_ptr<BaguetteDownload> {
            return std::make_unique<SimpleURLLoaderDownload>(local_state);
          },
          std::ref(local_state))),
      profile_(profile) {}
BaguetteInstaller::~BaguetteInstaller() = default;

void BaguetteInstaller::Install(BaguetteInstallerCallback callback) {
  installations_.push_back(std::make_unique<guest_os::GuestOsDlcInstallation>(
      kToolsDlcName,
      base::BindOnce(&BaguetteInstaller::OnInstallDlc,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      base::DoNothing()));
}

void BaguetteInstaller::OnInstallDlc(
    BaguetteInstallerCallback callback,
    guest_os::GuestOsDlcInstallation::Result result) {
  InstallResult response =
      result
          .transform_error(
              [](guest_os::GuestOsDlcInstallation::Error err) {
                switch (err) {
                  case guest_os::GuestOsDlcInstallation::Error::Cancelled:
                    return InstallResult::Cancelled;
                  case guest_os::GuestOsDlcInstallation::Error::Offline:
                    LOG(ERROR) << "Failed to install termina-tools-dlc while "
                                  "offline, assuming "
                                  "network issue.";
                    return InstallResult::Offline;
                  case guest_os::GuestOsDlcInstallation::Error::NeedUpdate:
                  case guest_os::GuestOsDlcInstallation::Error::NeedReboot:
                    LOG(ERROR)
                        << "Failed to install termina-tools-dlc because the OS "
                           "must be updated";
                    return InstallResult::NeedUpdate;
                  case guest_os::GuestOsDlcInstallation::Error::DiskFull:
                  case guest_os::GuestOsDlcInstallation::Error::Busy:
                  case guest_os::GuestOsDlcInstallation::Error::Internal:
                  case guest_os::GuestOsDlcInstallation::Error::Invalid:
                  case guest_os::GuestOsDlcInstallation::Error::UnknownFailure:
                    LOG(ERROR)
                        << "Failed to install termina-tools-dlc: " << err;
                    return InstallResult::Failure;
                }
              })
          .error_or(InstallResult::Success);

  if (response != InstallResult::Success) {
    std::move(callback).Run(response, {});
    return;
  }

  auto* concierge_client = ash::ConciergeClient::Get();
  concierge_client->WaitForServiceToBeAvailable(
      base::BindOnce(&BaguetteInstaller::OnConciergeAvailable,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BaguetteInstaller::OnConciergeAvailable(BaguetteInstallerCallback callback,
                                             bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "vm_concierge service is unavailable.";
    std::move(callback).Run(InstallResult::Failure, {});
  }

  // We always need to check if tools DLC is present for kernel, but we may
  // already have a baguette disk image, check for that first.
  auto* concierge_client = ash::ConciergeClient::Get();
  vm_tools::concierge::ListVmDisksRequest request;
  request.set_vm_name(kCrostiniDefaultVmName);
  request.set_cryptohome_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  concierge_client->ListVmDisks(
      request,
      base::BindOnce(&BaguetteInstaller::OnListVmDisks,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BaguetteInstaller::OnListVmDisks(
    BaguetteInstallerCallback callback,
    std::optional<vm_tools::concierge::ListVmDisksResponse> response) {
  if (!response) {
    LOG(WARNING) << "Unable to query disk image status, will potentially "
                    "re-download baguette image needlessly.";
  }
  if (!response->success()) {
    LOG(WARNING) << "Unsuccessful disk image response, will potentially "
                    "re-download baguette image needlessly.";
  }

  if (std::any_of(
          response->images().cbegin(), response->images().cend(),
          [](auto image) {
            return image.vm_type() ==
                   vm_tools::concierge::VmInfo_VmType::VmInfo_VmType_BAGUETTE;
          })) {
    std::move(callback).Run(InstallResult::Success, {});
    return;
  }

  auto* concierge_client = ash::ConciergeClient::Get();
  concierge_client->GetBaguetteImageUrl(
      base::BindOnce(&BaguetteInstaller::DownloadBaguetteImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BaguetteInstaller::DownloadBaguetteImage(
    BaguetteInstallerCallback callback,
    std::optional<vm_tools::concierge::GetBaguetteImageUrlResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to get baguette disk image URL from vm_concierge.";
    std::move(callback).Run(InstallResult::DownloadError, {});
    return;
  } else if (response->url().empty()) {
    LOG(ERROR) << "vm_concierge returned an empty baguette image URL.";
    std::move(callback).Run(InstallResult::DownloadError, {});
    return;
  } else if (response->sha256().empty()) {
    LOG(ERROR) << "vm_concierge returned an empty baguette image checksum.";
    std::move(callback).Run(InstallResult::DownloadError, {});
    return;
  }

  image_download_ = download_factory_.Run();
  image_download_->StartDownload(
      profile_, GURL(response->url()),
      base::BindOnce(&crostini::BaguetteInstaller::OnDiskImageDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     response->sha256()));
}

void BaguetteInstaller::OnDiskImageDownloaded(
    BaguetteInstallerCallback callback,
    std::string expected_hash,
    base::FilePath path,
    std::string hash) {
  if (path.empty()) {
    LOG(ERROR) << "Error downloading.";
    std::move(callback).Run(InstallResult::DownloadError, {});
    return;
  }
  if (!base::EqualsCaseInsensitiveASCII(hash, expected_hash)) {
    LOG(ERROR) << "Downloaded image has incorrect SHA256 hash.";
    std::move(callback).Run(InstallResult::ChecksumError, {});
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&OpenFdBlocking, path),
      base::BindOnce(&BaguetteInstaller::OnOpenFd,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BaguetteInstaller::OnOpenFd(BaguetteInstallerCallback callback,
                                 base::ScopedFD image) {
  if (!image.is_valid()) {
    LOG(ERROR) << "Unable to open image file.";
    std::move(callback).Run(InstallResult::Failure, {});
    return;
  }

  std::move(callback).Run(InstallResult::Success, std::move(image));
}

void BaguetteInstaller::Uninstall(base::OnceCallback<void(bool)> callback) {
  ash::DlcserviceClient::Get()->GetExistingDlcs(base::BindOnce(
      [](base::WeakPtr<BaguetteInstaller> weak_this,
         base::OnceCallback<void(bool)> callback, std::string_view err,
         const dlcservice::DlcsWithContent& dlcs_with_content) {
        if (!weak_this) {
          return;
        }

        if (err != dlcservice::kErrorNone) {
          LOG(ERROR) << "Failed to list installed DLCs: " << err;
          std::move(callback).Run(false);
          return;
        }
        for (const auto& dlc : dlcs_with_content.dlc_infos()) {
          if (dlc.id() == kToolsDlcName) {
            VLOG(1) << "DLC present, removing";
            weak_this->RemoveDlc(std::move(callback));
            return;
          }
        }
        VLOG(1) << "No DLC present, skipping";
        std::move(callback).Run(true);
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BaguetteInstaller::RemoveDlc(base::OnceCallback<void(bool)> callback) {
  ash::DlcserviceClient::Get()->Uninstall(
      kToolsDlcName,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback, std::string_view err) {
            if (err == dlcservice::kErrorNone) {
              VLOG(1) << "Removed DLC";
              std::move(callback).Run(true);
            } else {
              LOG(ERROR) << "Failed to remove termina-tools-dlc: " << err;
              std::move(callback).Run(false);
            }
          },
          std::move(callback)));
}

}  // namespace crostini
