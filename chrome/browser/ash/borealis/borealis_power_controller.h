// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_POWER_CONTROLLER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_POWER_CONTROLLER_H_

#include "ash/wm/window_state.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "ui/aura/client/focus_change_observer.h"

namespace borealis {

// Prevents the device from going to sleep/dimming when Borealis requests it.
// TODO(b/197591894): Make this more intelligent than just creating a wakelock
// whenever a borealis window is in focus.
class BorealisPowerController : public aura::client::FocusChangeObserver {
 public:
  BorealisPowerController();
  BorealisPowerController(const BorealisPowerController&) = delete;
  BorealisPowerController& operator=(const BorealisPowerController&) = delete;
  ~BorealisPowerController() override;

  // Overridden from FocusChangeObserver
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

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
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_POWER_CONTROLLER_H_
