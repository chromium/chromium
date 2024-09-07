// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_BACKGROUND_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_BACKGROUND_SERVICE_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/vmm/arc_system_state_bridge.h"
#include "chromeos/ash/components/throttle/throttle_observer.h"

namespace arc {

constexpr char kArcBackgroundServiceObserverName[] =
    "ArcBackgroundServiceObserver";

// This class observes ARC app background services state and sets the state to
// active when there are critial app background services running.
class ArcBackgroundServiceObserver : public ash::ThrottleObserver,
                                     public ArcSystemStateBridge::Observer {
 public:
  ArcBackgroundServiceObserver();

  ArcBackgroundServiceObserver(const ArcBackgroundServiceObserver&) = delete;
  ArcBackgroundServiceObserver& operator=(const ArcBackgroundServiceObserver&) =
      delete;

  ~ArcBackgroundServiceObserver() override;

  // ash::ThrottleObserver:
  void StartObserving(content::BrowserContext*,
                      const ObserverStateChangedCallback& callback) override;
  // Callers must call StopObserving() once for every use of StartObserving(),
  // and must not destroy this object while in Observing state.
  void StopObserving() override;

  // ArcSystemStateBridge::Observer:
  void OnArcSystemAppRunningStateChange(
      const mojom::SystemAppRunningState& state) override;

 private:
  raw_ptr<content::BrowserContext> context_ = nullptr;

  base::ScopedObservation<ArcSystemStateBridge, ArcSystemStateBridge::Observer>
      observation_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_BACKGROUND_SERVICE_OBSERVER_H_
