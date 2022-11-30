// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_POWER_CONTROLLER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_POWER_CONTROLLER_H_

#include <set>
#include "ash/wm/window_state.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window.h"

class Profile;

namespace borealis {

// Prevents the device from going to sleep/dimming when Borealis requests it.
// Conditions for this are either the Steam client is focused or the VM
// sends an inhibit message.
// TODO(b/244273692): Remove the window focus logic once download signals
// are available.
class BorealisPowerController : public aura::client::FocusChangeObserver,
                                ash::CiceroneClient::Observer {
 public:
  explicit BorealisPowerController(Profile* profile);
  BorealisPowerController(const BorealisPowerController&) = delete;
  BorealisPowerController& operator=(const BorealisPowerController&) = delete;
  ~BorealisPowerController() override;

  // Overridden from FocusChangeObserver
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // ash::CiceroneClient::Observer override.
  void OnInhibitScreensaver(
      const vm_tools::cicerone::InhibitScreensaverSignal& signal) override;

  // ash::CiceroneClient::Observer override.
  void OnUninhibitScreensaver(
      const vm_tools::cicerone::UninhibitScreensaverSignal& signal) override;

  void EnsureWakeLock();

  void SetWakeLockProviderForTesting(
      mojo::Remote<device::mojom::WakeLockProvider> provider) {
    wake_lock_provider_ = std::move(provider);
  }

  void FlushForTesting() {
    if (wake_lock_) {
      wake_lock_.FlushForTesting();
    }
  }

 private:
  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider_;
  mojo::Remote<device::mojom::WakeLock> wake_lock_;
  // Cookies from Inhibit messages that have not yet received uninhibit.
  std::set<int64_t> cookies_;
  Profile* const profile_;
  std::string const owner_id_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_POWER_CONTROLLER_H_
