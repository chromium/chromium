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
#include "ui/aura/window.h"

class Profile;

namespace borealis {

// Prevents the device from going to sleep when the VM
// receives an inhibit message. Allows screen off if reason is "download"
// otherwise, keeps screen on.
class BorealisPowerController : public ash::CiceroneClient::Observer {
 public:
  explicit BorealisPowerController(Profile* profile);
  BorealisPowerController(const BorealisPowerController&) = delete;
  BorealisPowerController& operator=(const BorealisPowerController&) = delete;
  ~BorealisPowerController() override;

  // ash::CiceroneClient::Observer override.
  void OnInhibitScreensaver(
      const vm_tools::cicerone::InhibitScreensaverSignal& signal) override;

  // ash::CiceroneClient::Observer override.
  void OnUninhibitScreensaver(
      const vm_tools::cicerone::UninhibitScreensaverSignal& signal) override;

  void SetWakeLockProviderForTesting(
      mojo::Remote<device::mojom::WakeLockProvider> provider) {
    wake_lock_provider_ = std::move(provider);
  }

  void FlushForTesting() {
    if (wake_lock_) {
      wake_lock_.FlushForTesting();
    }
    if (download_wake_lock_) {
      download_wake_lock_.FlushForTesting();
    }
  }

 private:
  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider_;
  mojo::Remote<device::mojom::WakeLock> wake_lock_;
  mojo::Remote<device::mojom::WakeLock> download_wake_lock_;
  // Cookies from Inhibit messages that have not yet received uninhibit.
  std::set<u_int32_t> cookies_;
  std::set<u_int32_t> download_cookies_;
  std::string const owner_id_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_POWER_CONTROLLER_H_
