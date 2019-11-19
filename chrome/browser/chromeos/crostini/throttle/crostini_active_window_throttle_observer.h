// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_THROTTLE_CROSTINI_ACTIVE_WINDOW_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_THROTTLE_CROSTINI_ACTIVE_WINDOW_THROTTLE_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/window_throttle_observer_base.h"

namespace crostini {

// This class observes window activations and sets the state to active if the
// currently active window is a Crostini window.
class CrostiniActiveWindowThrottleObserver
    : public chromeos::WindowThrottleObserverBase {
 public:
  CrostiniActiveWindowThrottleObserver();
  ~CrostiniActiveWindowThrottleObserver() override = default;

  // WindowThrottleObserverBase:
  bool ProcessWindowActivation(ActivationReason reason,
                               aura::Window* gained_active,
                               aura::Window* lost_active) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniActiveWindowThrottleObserver);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_THROTTLE_CROSTINI_ACTIVE_WINDOW_THROTTLE_OBSERVER_H_
