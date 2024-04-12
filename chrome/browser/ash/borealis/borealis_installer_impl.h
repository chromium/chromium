// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_INSTALLER_IMPL_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_INSTALLER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_types.mojom-forward.h"
#include "chrome/browser/ash/borealis/infra/described.h"

class Profile;

namespace borealis {

// This class is responsible for installing the Borealis VM. Currently
// the only installation requirements for Borealis is to install the
// relevant DLC component. The installer works closely with
// chrome/browser/ui/webui/ash/borealis_installer/
// borealis_installer_page_handler.cc.
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

  // Removes borealis and all of its associated apps/features from the system.
  void Uninstall(base::OnceCallback<void(BorealisUninstallResult)>
                     on_uninstall_callback) override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  // Holds information about (un)install operations.
  struct InstallInfo {
    std::string vm_name;
    std::string container_name;
  };

  // Classes which represent the transition between installed and not-installed.
  class Installation;
  class Uninstallation;

  void UpdateProgress(double state_progress);
  void UpdateInstallingState(InstallingState installing_state);

  void OnInstallComplete(
      base::expected<std::unique_ptr<InstallInfo>,
                     Described<mojom::InstallResult>> result_or_error);
  void OnUninstallComplete(
      base::OnceCallback<void(BorealisUninstallResult)> on_uninstall_callback,
      base::expected<std::unique_ptr<InstallInfo>, BorealisUninstallResult>
          result);

  raw_ptr<Profile> profile_;
  base::ObserverList<Observer> observers_;

  InstallingState installing_state_;

  std::unique_ptr<Installation> in_progress_installation_;
  std::unique_ptr<Uninstallation> in_progress_uninstallation_;

  base::WeakPtrFactory<BorealisInstallerImpl> weak_ptr_factory_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_INSTALLER_IMPL_H_
