// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_active_window_throttle_observer.h"

#include "ash/public/cpp/app_types_util.h"

namespace arc {

ArcActiveWindowThrottleObserver::ArcActiveWindowThrottleObserver()
    : WindowThrottleObserverBase(kArcActiveWindowThrottleObserverName) {}

bool ArcActiveWindowThrottleObserver::ProcessWindowActivation(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  return ash::IsArcWindow(gained_active);
}

}  // namespace arc
