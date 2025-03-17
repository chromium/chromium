// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_BAGUETTE_INSTALLER_H_
#define CHROME_BROWSER_ASH_CROSTINI_BAGUETTE_INSTALLER_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"

// TODO(crbug.com/377377749): add downloader which grabs image file from GS
// bucket based on VERSION-PIN
inline constexpr char kBaguettePath[] =
    "/home/chronos/user/MyFiles/Downloads/baguette.img.zst";

namespace crostini {

// This class is responsible for managing (un)instatllation of Baguette - the
// containerless Crostini VM.
class BaguetteInstaller {
 public:
  BaguetteInstaller();
  ~BaguetteInstaller();

  BaguetteInstaller(const BaguetteInstaller&) = delete;
  BaguetteInstaller& operator=(const BaguetteInstaller&) = delete;

  enum class InstallResult {
    // The install succeeded.
    Success,
    // The install failed for an unspecified reason.
    Failure,
    // The install failed because it needed to download an image and the device
    // is offline.
    Offline,
    // The device must be updated before termina can be installed.
    NeedUpdate,
    // The install request was cancelled.
    Cancelled,
  };

  void Install(base::OnceCallback<void(InstallResult)> callback);

 private:
  void OnInstallDlc(base::OnceCallback<void(InstallResult)> callback,
                    guest_os::GuestOsDlcInstallation::Result result);

  std::vector<std::unique_ptr<guest_os::GuestOsDlcInstallation>> installations_;

  base::WeakPtrFactory<BaguetteInstaller> weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_BAGUETTE_INSTALLER_H_
