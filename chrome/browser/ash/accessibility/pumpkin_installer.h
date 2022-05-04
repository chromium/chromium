// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_PUMPKIN_INSTALLER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_PUMPKIN_INSTALLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

namespace ash {

// This class encapsulates all logic involving the installation of the Pumpkin
// DLC. It communicates install success, progress, and error using callbacks
// that are provided at construction. Note: Pumpkin is a semantic parser which
// is currently used by accessibility services on ChromeOS.
class PumpkinInstaller {
  using InstalledCallback =
      base::RepeatingCallback<void(const std::string& root_path)>;
  using ProgressCallback = base::RepeatingCallback<void(double progress)>;
  using ErrorCallback = base::RepeatingCallback<void(const std::string& error)>;

 public:
  PumpkinInstaller(const InstalledCallback& on_installed,
                   const ProgressCallback& on_progress,
                   const ErrorCallback& on_error);
  ~PumpkinInstaller();
  PumpkinInstaller(const PumpkinInstaller&) = delete;
  PumpkinInstaller& operator=(const PumpkinInstaller&) = delete;

  // Installs Pumpkin if it isn't already downloaded.
  void MaybeInstall();

 private:
  // A helper function that is run once we've grabbed the state of the Pumpkin
  // DLC from the DLC service.
  void MaybeInstallHelper(const std::string& error,
                          const dlcservice::DlcState& dlc_state);
  void OnInstalled(
      const chromeos::DlcserviceClient::InstallResult& install_result);
  void OnProgress(double progress);
  void OnError(const std::string& error);
  base::WeakPtr<PumpkinInstaller> GetWeakPtr();

  // A callback that is run when Pumpkin is installed.
  InstalledCallback on_installed_;
  // A callback that is run when Pumpkin download progress is updated.
  ProgressCallback on_progress_;
  // A callback that is run when Pumpkin download encounters an error.
  ErrorCallback on_error_;
  // Requests to DlcserviceClient are async. This is true if we've made a
  // request and are still waiting for a response.
  bool pending_dlc_request_;

  base::WeakPtrFactory<PumpkinInstaller> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_PUMPKIN_INSTALLER_H_
