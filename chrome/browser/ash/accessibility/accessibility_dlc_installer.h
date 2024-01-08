// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_DLC_INSTALLER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_DLC_INSTALLER_H_

#include <set>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

namespace ash {

// This class encapsulates all logic involving the installation of accessibility
// DLCs. It communicates install success, progress, and error using callbacks.
class AccessibilityDlcInstaller {
  // TODO(akihiroota): Add the install path as a callback parameter when we have
  // a specific need for it.
  using InstalledCallback = base::OnceCallback<void(const bool success)>;
  using ProgressCallback = base::RepeatingCallback<void(double progress)>;
  using ErrorCallback = base::OnceCallback<void(const std::string& error)>;

 public:
  // TODO(b/309121742): Add support for the FaceGaze DLC.
  enum class DlcType { kPumpkin };

  AccessibilityDlcInstaller();
  ~AccessibilityDlcInstaller();
  AccessibilityDlcInstaller(const AccessibilityDlcInstaller&) = delete;
  AccessibilityDlcInstaller& operator=(const AccessibilityDlcInstaller&) =
      delete;

  // Installs a DLC if it isn't already downloaded.
  void MaybeInstall(DlcType type,
                    InstalledCallback on_installed,
                    ProgressCallback on_progress,
                    ErrorCallback on_error);

  bool IsPumpkinInstalled() const;

 private:
  // A helper function that is run once we've grabbed the state of a DLC from
  // the DLC service.
  void MaybeInstallHelper(DlcType type,
                          const std::string& error,
                          const dlcservice::DlcState& dlc_state);
  void OnInstalled(DlcType type,
                   const DlcserviceClient::InstallResult& install_result);
  void OnProgress(double progress);
  void OnError(const std::string& error);

  std::string GetDlcName(DlcType type);
  std::string GetDlcInstallingErrorMessage(DlcType type);
  std::string GetPendingDlcRequestErrorMessage(DlcType type);

  base::WeakPtr<AccessibilityDlcInstaller> GetWeakPtr();

  std::set<DlcType> installed_dlcs_;

  // A callback that is run when a DLC is installed.
  InstalledCallback on_installed_;
  // A callback that is run when DLC download progress is updated.
  ProgressCallback on_progress_;
  // A callback that is run when a DLC download encounters an error.
  ErrorCallback on_error_;
  // Requests to DlcserviceClient are async. This is true if we've made a
  // request and are still waiting for a response.
  bool pending_dlc_request_ = false;

  base::WeakPtrFactory<AccessibilityDlcInstaller> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_DLC_INSTALLER_H_
