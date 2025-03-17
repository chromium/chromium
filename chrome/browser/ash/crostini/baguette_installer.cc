// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/baguette_installer.h"

#include <algorithm>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
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
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"

namespace crostini {

BaguetteInstaller::BaguetteInstaller() = default;
BaguetteInstaller::~BaguetteInstaller() = default;

void BaguetteInstaller::Install(
    base::OnceCallback<void(InstallResult)> callback) {
  installations_.push_back(std::make_unique<guest_os::GuestOsDlcInstallation>(
      kToolsDlcName,
      base::BindOnce(&BaguetteInstaller::OnInstallDlc,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      base::DoNothing()));
}

void BaguetteInstaller::OnInstallDlc(
    base::OnceCallback<void(InstallResult)> callback,
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

  if (response == InstallResult::Success) {
    // This will eventually download the image from a storage bucket, but for
    // now we know it should be located in MyFiles/Downloads
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()}, base::BindOnce([]() {
          // check for file existence
          if (!base::PathExists(base::FilePath(kBaguettePath))) {
            LOG(ERROR) << "Couldn't find " << kBaguettePath;
            return InstallResult::Failure;
          }
          return InstallResult::Success;
        }),
        std::move(callback));

  } else {
    std::move(callback).Run(response);
  }
}

}  // namespace crostini
