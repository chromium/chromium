// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_THROTTLE_CROSTINI_ACTIVE_WINDOW_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_CROSTINI_THROTTLE_CROSTINI_ACTIVE_WINDOW_THROTTLE_OBSERVER_H_

#include "chromeos/ash/components/throttle/window_throttle_observer_base.h"

namespace crostini {

// This class observes window activations and sets the state to active if the
// currently active window is a Crostini window.
class CrostiniActiveWindowThrottleObserver
    : public ash::WindowThrottleObserverBase {
 public:
  CrostiniActiveWindowThrottleObserver();

  CrostiniActiveWindowThrottleObserver(
      const CrostiniActiveWindowThrottleObserver&) = delete;
  CrostiniActiveWindowThrottleObserver& operator=(
      const CrostiniActiveWindowThrottleObserver&) = delete;

  ~CrostiniActiveWindowThrottleObserver() override = default;

  // WindowThrottleObserverBase:
  bool ProcessWindowActivation(ActivationReason reason,
                               aura::Window* gained_active,
                               aura::Window* lost_active) override;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_THROTTLE_CROSTINI_ACTIVE_WINDOW_THROTTLE_OBSERVER_H_
