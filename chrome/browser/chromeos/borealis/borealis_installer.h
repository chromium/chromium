// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"

namespace borealis {

class BorealisInstaller : public KeyedService {
 public:
  enum class InstallationResult {
    kCompleted,
    kCancelled,
    kNotAllowed,
    kOperationInProgress,
    kDlcInternal,
    kDlcUnsupported,
    kDlcBusy,
    kDlcNeedReboot,
    kDlcNeedSpace,
    kDlcUnknown,
  };

  enum class InstallingState {
    kInactive,
    kInstallingDlc,
  };

  // Observer class for the Borealis installation related events.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnProgressUpdated(double fraction_complete) = 0;
    virtual void OnStateUpdated(InstallingState new_state) = 0;
    virtual void OnInstallationEnded(InstallationResult result) = 0;
    virtual void OnCancelInitiated() = 0;
  };

  BorealisInstaller();

  static std::string GetInstallingStateName(InstallingState state);

  // Checks if an installation process is already running.
  virtual bool IsProcessing() = 0;
  // Start the installation process.
  virtual void Start() = 0;
  // Cancels the installation process.
  virtual void Cancel() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  ~BorealisInstaller() override;

  base::ObserverList<Observer> observers_;

 private:
  base::WeakPtrFactory<BorealisInstaller> weak_ptr_factory_{this};
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_H_
