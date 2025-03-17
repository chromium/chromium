// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/baguette_installer.h"

#include <algorithm>
#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/crostini/baguette_download.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "url/origin.h"

namespace crostini {

// TODO(crbug.com/377377749): replace with url from concierge rpc
constexpr char baguetteUrl[] =
    "https://storage.googleapis.com/cros-containers/baguette/images/"
    "baguette_rootfs_amd64_2025-01-29-000057_"
    "6310e875487f154a58648db8fb3cc284401f856e.img.zstd";
// TODO(crbug.com/377377749): replace with sha256 from concierge rpc
constexpr char baguetteSHA256[] =
    "e21336031b00057afd4f3414369cbf98d8e12783cb38a98cd12f7b9318bdc443";

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

  DownloadBaguetteImage(std::move(callback));
}

void BaguetteInstaller::DownloadBaguetteImage(
    BaguetteInstallerCallback callback) {
  image_download_ = download_factory_.Run();
  image_download_->StartDownload(
      profile_, GURL(baguetteUrl),
      base::BindOnce(&crostini::BaguetteInstaller::OnDiskImageDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BaguetteInstaller::OnDiskImageDownloaded(
    BaguetteInstallerCallback callback,
    base::FilePath path,
    std::string hash) {
  if (path.empty()) {
    LOG(ERROR) << "Error downloading.";
    std::move(callback).Run(InstallResult::DownloadError, {});
    return;
  }
  if (!base::EqualsCaseInsensitiveASCII(hash, baguetteSHA256)) {
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

}  // namespace crostini
