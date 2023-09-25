// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_INSTALLER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_INSTALLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_types.mojom-forward.h"
#include "components/keyed_service/core/keyed_service.h"

namespace borealis {

class BorealisInstaller : public KeyedService {
 public:
  enum class InstallingState {
    kInactive,
    kCheckingIfAllowed,
    kInstallingDlc,
    kStartingUp,
    kAwaitingApplications,
  };

  // Observer class for the Borealis installation related events.
  //
  // TODO(b/189720611): It is not possible to have more than one observer, so
  // refactor this to use callbacks rather than observers.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnProgressUpdated(double fraction_complete) = 0;
    virtual void OnStateUpdated(InstallingState new_state) = 0;
    // Called when installation succeeds/fails, per |result|. If it fails,
    // |error_description| contains a string useful for debugging/understanding
    // the cause of the failure, not for end-users.
    virtual void OnInstallationEnded(mojom::InstallResult result,
                                     const std::string& error_description) = 0;
    virtual void OnCancelInitiated() = 0;
  };

  BorealisInstaller();
  ~BorealisInstaller() override;

  static std::string GetInstallingStateName(InstallingState state);

  // Checks if an installation process is already running. This applies to
  // installation only, uninstallation is unaffected (also for Start() and
  // Cancel()).
  virtual bool IsProcessing() = 0;
  // Start the installation process.
  virtual void Start() = 0;
  // Cancels the installation process.
  virtual void Cancel() = 0;

  // Removes borealis and all of its associated apps/features from the system.
  virtual void Uninstall(base::OnceCallback<void(BorealisUninstallResult)>
                             on_uninstall_callback) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_INSTALLER_H_
