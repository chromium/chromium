// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_ACTIVE_WINDOW_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_ACTIVE_WINDOW_THROTTLE_OBSERVER_H_

#include "chromeos/ash/components/throttle/window_throttle_observer_base.h"

namespace arc {

namespace {
constexpr char kArcActiveWindowThrottleObserverName[] =
    "ArcWindowIsActiveWindow";
}  // namespace

// This class observes window activations and sets the state to active if the
// currently active window is an ARC window.
class ArcActiveWindowThrottleObserver : public ash::WindowThrottleObserverBase {
 public:
  ArcActiveWindowThrottleObserver();

  ArcActiveWindowThrottleObserver(const ArcActiveWindowThrottleObserver&) =
      delete;
  ArcActiveWindowThrottleObserver& operator=(
      const ArcActiveWindowThrottleObserver&) = delete;

  ~ArcActiveWindowThrottleObserver() override = default;

  // WindowThrottleObserverBase:
  bool ProcessWindowActivation(ActivationReason reason,
                               aura::Window* gained_active,
                               aura::Window* lost_active) override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_ACTIVE_WINDOW_THROTTLE_OBSERVER_H_
