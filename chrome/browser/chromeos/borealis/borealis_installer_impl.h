// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_IMPL_H_

#include "chrome/browser/chromeos/borealis/borealis_installer.h"

#include <memory>

#include "chrome/browser/chromeos/borealis/borealis_metrics.h"
#include "chrome/browser/chromeos/borealis/infra/expected.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

class Profile;

namespace borealis {

class Uninstallation;

// This class is responsible for installing the Borealis VM. Currently
// the only installation requirements for Borealis is to install the
// relevant DLC component. The installer works with closesly with
// chrome/browser/ui/views/borealis/borealis_installer_view.h.
class BorealisInstallerImpl : public BorealisInstaller {
 public:
  explicit BorealisInstallerImpl(Profile* profile);
  ~BorealisInstallerImpl() override;

  // Disallow copy and assign.
  BorealisInstallerImpl(const BorealisInstallerImpl&) = delete;
  BorealisInstallerImpl& operator=(const BorealisInstallerImpl&) = delete;

  // Checks if an installation process is already running.
  bool IsProcessing() override;
  // Start the installation process.
  void Start() override;
  // Cancels the installation process.
  void Cancel() override;

  // Holds information about uninstall operations.
  struct UninstallInfo {
    std::string vm_name;
    std::string container_name;
    base::Time start_time;
  };

  // Removes borealis and all of its associated apps/features from the system.
  void Uninstall(base::OnceCallback<void(BorealisUninstallResult)>
                     on_uninstall_callback) override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  enum class State {
    kIdle,
    kInstalling,
    kCancelling,
  };

  void StartDlcInstallation();
  void InstallationEnded(BorealisInstallResult result);

  void UpdateProgress(double state_progress);
  void UpdateInstallingState(InstallingState installing_state);

  void OnDlcInstallationProgressUpdated(double progress);
  void OnDlcInstallationCompleted(
      const chromeos::DlcserviceClient::InstallResult& install_result);

  void OnUninstallComplete(
      base::OnceCallback<void(BorealisUninstallResult)> on_uninstall_callback,
      Expected<std::unique_ptr<UninstallInfo>, BorealisUninstallResult> result);

  State state_;
  InstallingState installing_state_;
  double progress_;
  base::TimeTicks installation_start_tick_;
  Profile* profile_;
  base::ObserverList<Observer> observers_;

  std::unique_ptr<Uninstallation> in_progress_uninstallation_;

  base::WeakPtrFactory<BorealisInstallerImpl> weak_ptr_factory_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_IMPL_H_
