// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_TERMINA_INSTALLER_H_
#define CHROME_BROWSER_ASH_CROSTINI_TERMINA_INSTALLER_H_

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"

namespace crostini {

// This class is responsible managing (un)installation of the Termina VM.
// From M105, we require using DLC and will forcibly remove the component even
// if loading DLC fails.
class TerminaInstaller {
 public:
  TerminaInstaller();
  ~TerminaInstaller();

  TerminaInstaller(const TerminaInstaller&) = delete;
  TerminaInstaller& operator=(const TerminaInstaller&) = delete;

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

  // This is really a bool, but std::vector<bool> has weird properties that stop
  // us from using bool here.
  using UninstallResult = int;

  // Ensure that termina is installed. This will also attempt to remove any
  // other instances of termina that may be installed, but will not block on or
  // check the result of this.
  void Install(base::OnceCallback<void(InstallResult)> callback);

  // Remove termina entirely. This will also attempt to remove any
  // other instances of termina that may be installed.
  void Uninstall(base::OnceCallback<void(bool)> callback);

  // Get a path to the install location of termina. You must call Install and
  // get a Success response back before calling this method.
  base::FilePath GetInstallLocation();

  // Get the id of the installed DLC, or nullopt if DLC is not being used.
  std::optional<std::string> GetDlcId();

  // Attempt to cancel a pending install. The DLC service does not support
  // this, but we have some retry logic that can be aborted. The result
  // callback is run on completion with a result of Cancelled.
  // If there are multiple concurrent install requests, the wrong request may
  // end up cancelled.
  void CancelInstall();

 private:
  void OnInstallDlc(base::OnceCallback<void(InstallResult)> callback,
                    guest_os::GuestOsDlcInstallation::Result result);

  void RemoveComponentIfPresent(base::OnceCallback<void()> callback,
                                UninstallResult* result);
  void RemoveDlcIfPresent(base::OnceCallback<void()> callback,
                          UninstallResult* result);
  void RemoveDlc(base::OnceCallback<void()> callback, UninstallResult* result);

  void OnUninstallFinished(base::OnceCallback<void(bool)> callback,
                           std::vector<UninstallResult> partial_results);

  std::optional<base::FilePath> termina_location_{std::nullopt};
  std::optional<std::string> dlc_id_{};

  // Crostini will potentially enqueue multiple installations.
  //
  // TODO(b/277835995): This is probably not intended, this allows you to cancel
  //                    a concurrent installation. Consider changing to enforce
  //                    single installations.
  std::vector<std::unique_ptr<guest_os::GuestOsDlcInstallation>> installations_;

  base::WeakPtrFactory<TerminaInstaller> weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_TERMINA_INSTALLER_H_
