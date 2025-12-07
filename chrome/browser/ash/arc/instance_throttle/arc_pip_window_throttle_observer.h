// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_PIP_WINDOW_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_PIP_WINDOW_THROTTLE_OBSERVER_H_

#include "chromeos/ash/components/throttle/throttle_observer.h"
#include "ui/aura/window_observer.h"

namespace content {
class BrowserContext;
}

namespace arc {

// This class observes the PIP window container and sets the state to active if
// an ARC PIP window is currently visible.
class ArcPipWindowThrottleObserver : public ash::ThrottleObserver,
                                     public aura::WindowObserver {
 public:
  ArcPipWindowThrottleObserver();
  ~ArcPipWindowThrottleObserver() override = default;
  ArcPipWindowThrottleObserver(const ArcPipWindowThrottleObserver&) = delete;
  ArcPipWindowThrottleObserver& operator=(const ArcPipWindowThrottleObserver&) =
      delete;

  // ash::ThrottleObserver:
  void StartObserving(content::BrowserContext*,
                      const ObserverStateChangedCallback& callback) override;
  void StopObserving() override;

  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* window) override;
  void OnWindowRemoved(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_PIP_WINDOW_THROTTLE_OBSERVER_H_
