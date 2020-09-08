// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_IMPL_H_

#include "chrome/browser/chromeos/borealis/borealis_installer.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

namespace borealis {

// This class is responsible for installing the Borealis VM. Currently
// the only installation requirements for Borealis is to install the
// relevant DLC component. The installer works with closesly with
// chrome/browser/ui/views/borealis/borealis_installer_view.h.
class BorealisInstallerImpl : public BorealisInstaller {
 public:
  BorealisInstallerImpl();

  // Disallow copy and assign.
  BorealisInstallerImpl(const BorealisInstallerImpl&) = delete;
  BorealisInstallerImpl& operator=(const BorealisInstallerImpl&) = delete;

  // Checks if an installation process is already running.
  bool IsProcessing() override;
  // Start the installation process.
  void Start() override;
  // Cancels the installation process.
  void Cancel() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  enum class State {
    kIdle,
    kInstalling,
    kCancelling,
  };

  ~BorealisInstallerImpl() override;

  void StartDlcInstallation();
  void InstallationEnded(InstallationResult result);

  void UpdateProgress(double state_progress);
  void UpdateInstallingState(InstallingState installing_state);

  void OnDlcInstallationProgressUpdated(double progress);
  void OnDlcInstallationCompleted(
      const chromeos::DlcserviceClient::InstallResult& install_result);

  State state_ = State::kIdle;
  InstallingState installing_state_ = InstallingState::kInactive;
  double progress_ = 0;

  base::WeakPtrFactory<BorealisInstallerImpl> weak_ptr_factory_{this};
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_IMPL_H_
