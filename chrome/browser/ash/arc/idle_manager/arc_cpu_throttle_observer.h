// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_CPU_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_CPU_THROTTLE_OBSERVER_H_

#include "chromeos/ash/components/throttle/throttle_observer.h"
#include "chromeos/ash/components/throttle/throttle_service.h"

namespace arc {

constexpr char kArcCpuThrottleObserverName[] = "ArcCpuThrottleObserver";

// This class observes ARCVM's instance throttle and sets the state to active
// whenever ARCVM is NOT being throttled.
class ArcCpuThrottleObserver : public ash::ThrottleObserver,
                               public ash::ThrottleService::ServiceObserver {
 public:
  ArcCpuThrottleObserver();

  ArcCpuThrottleObserver(const ArcCpuThrottleObserver&) = delete;
  ArcCpuThrottleObserver& operator=(const ArcCpuThrottleObserver&) = delete;

  ~ArcCpuThrottleObserver() override = default;

  // ash::ThrottleObserver:
  void StartObserving(content::BrowserContext*,
                      const ObserverStateChangedCallback& callback) override;
  // Callers must call StopObserving() once for every use of StartObserving(),
  // and must not destroy this object while in Observing state.
  void StopObserving() override;

  // ash:ThrottleService::ServiceObserver:
  void OnThrottle(bool throttled) override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_CPU_THROTTLE_OBSERVER_H_
