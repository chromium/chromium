// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/instance_throttle/arc_active_window_throttle_observer.h"

#include "components/arc/arc_util.h"

namespace arc {

ArcActiveWindowThrottleObserver::ArcActiveWindowThrottleObserver()
    : WindowThrottleObserverBase(ThrottleObserver::PriorityLevel::CRITICAL,
                                 "ArcWindowIsActiveWindow") {}

bool ArcActiveWindowThrottleObserver::ProcessWindowActivation(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  return IsArcAppWindow(gained_active);
}

}  // namespace arc
