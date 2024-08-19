// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_POWER_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_POWER_THROTTLE_OBSERVER_H_

#include "ash/components/arc/power/arc_power_bridge.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/throttle/throttle_observer.h"

namespace arc {

constexpr char kArcPowerThrottleObserverName[] = "ArcPower";

// Listens ARC power events and lifts CPU throttling when needed.
class ArcPowerThrottleObserver : public ash::ThrottleObserver,
                                 public ArcPowerBridge::Observer {
 public:
  ArcPowerThrottleObserver();

  ArcPowerThrottleObserver(const ArcPowerThrottleObserver&) = delete;
  ArcPowerThrottleObserver& operator=(const ArcPowerThrottleObserver&) = delete;

  ~ArcPowerThrottleObserver() override;

  // chromeos::ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;
  void StopObserving() override;

  // ArcPowerBridge::Observer:
  void OnPreAnr(mojom::AnrType type) override;
  void OnWillDestroyArcPowerBridge() override;

 private:
  base::OneShotTimer timer_;

  base::ScopedObservation<ArcPowerBridge, ArcPowerBridge::Observer>
      powerbridge_observation_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_POWER_THROTTLE_OBSERVER_H_
