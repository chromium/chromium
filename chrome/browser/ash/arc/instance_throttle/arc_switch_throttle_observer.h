// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_SWITCH_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_SWITCH_THROTTLE_OBSERVER_H_

#include "chromeos/ash/components/throttle/throttle_observer.h"

namespace arc {

// This disables ARC throttling in case it is explicitly set by a switch.
class ArcSwitchThrottleObserver : public ash::ThrottleObserver {
 public:
  ArcSwitchThrottleObserver();

  ArcSwitchThrottleObserver(const ArcSwitchThrottleObserver&) = delete;
  ArcSwitchThrottleObserver& operator=(const ArcSwitchThrottleObserver&) =
      delete;

  ~ArcSwitchThrottleObserver() override = default;

  // chromeos::ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;
};

}  // namespace arc
#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_SWITCH_THROTTLE_OBSERVER_H_
