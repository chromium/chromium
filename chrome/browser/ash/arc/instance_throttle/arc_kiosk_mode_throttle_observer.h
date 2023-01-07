// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_KIOSK_MODE_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_KIOSK_MODE_THROTTLE_OBSERVER_H_

#include "chrome/browser/ash/throttle_observer.h"

namespace arc {

// This disables ARC throttling in case of Kiosk mode where ARC is only one
// component.
// TODO(b/201683917): Maybe rename and use this lock for other special cases
// like demo mode.
class ArcKioskModeThrottleObserver : public ash::ThrottleObserver {
 public:
  ArcKioskModeThrottleObserver();

  ArcKioskModeThrottleObserver(const ArcKioskModeThrottleObserver&) = delete;
  ArcKioskModeThrottleObserver& operator=(const ArcKioskModeThrottleObserver&) =
      delete;

  ~ArcKioskModeThrottleObserver() override = default;

  // ash::ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;
  void StopObserving() override;
};

}  // namespace arc
#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_KIOSK_MODE_THROTTLE_OBSERVER_H_
