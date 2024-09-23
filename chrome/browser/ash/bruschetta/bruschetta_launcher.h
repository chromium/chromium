// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_LAUNCHER_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_LAUNCHER_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"

class Profile;

namespace bruschetta {

// Launches Bruschetta. One instance per VM.
class BruschettaLauncher {
 public:
  BruschettaLauncher(std::string vm_name, Profile* profile);
  virtual ~BruschettaLauncher();
  BruschettaLauncher(const BruschettaLauncher&) = delete;
  BruschettaLauncher& operator=(const BruschettaLauncher&) = delete;

  // Launches the Bruschetta instance this launcher controls if it's not already
  // running. Calls `callback` once Bruschetta is running or upon failure with
  // the result of the launch. Must be called on the UI thread.
  virtual void EnsureRunning(
      base::OnceCallback<void(BruschettaResult)> callback);

  // Gets a weak pointer to self.
  base::WeakPtr<BruschettaLauncher> GetWeakPtr();

 private:
  class Timeout;
  void EnsureConciergeAvailable();
  void OnConciergeAvailable(bool service_is_available);
  void StartVm();
  void OnStartVm(RunningVmPolicy launch_policy,
                 std::optional<vm_tools::concierge::StartVmResponse> response);

  void EnsureToolsDlcInstalled();
  void OnMountToolsDlc(guest_os::GuestOsDlcInstallation::Result install_result);

  void EnsureFirmwareDlcInstalled();
  void OnMountFirmwareDlc(
      guest_os::GuestOsDlcInstallation::Result install_result);

  void OnContainerRunning(guest_os::GuestInfo info);

  void OnTimeout();
  void Finish(BruschettaResult result);

  std::string vm_name_;
  raw_ptr<Profile> profile_;

  // Callbacks to run once an in-progress launch finishes.
  base::OnceCallbackList<void(BruschettaResult)> callbacks_;
  std::optional<base::CallbackListSubscription> subscription_;

  std::unique_ptr<guest_os::GuestOsDlcInstallation> in_progress_dlc_;
  std::unique_ptr<Timeout> timeout_;

  // Must be last.
  base::WeakPtrFactory<BruschettaLauncher> weak_factory_{this};
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_LAUNCHER_H_
