// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARCVM_KIOSK_MODE_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARCVM_KIOSK_MODE_THROTTLE_OBSERVER_H_

#include "chromeos/ash/components/throttle/throttle_observer.h"

namespace arc {

// Disables ARCVM throttling in case of Kiosk mode where ARCVM is only one
// component.
class ArcvmKioskModeThrottleObserver : public ash::ThrottleObserver {
 public:
  ArcvmKioskModeThrottleObserver();

  ArcvmKioskModeThrottleObserver(const ArcvmKioskModeThrottleObserver&) =
      delete;
  ArcvmKioskModeThrottleObserver& operator=(
      const ArcvmKioskModeThrottleObserver&) = delete;

  ~ArcvmKioskModeThrottleObserver() override = default;

  // ash::ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;

  void StopObserving() override;
};

}  // namespace arc
#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARCVM_KIOSK_MODE_THROTTLE_OBSERVER_H_
