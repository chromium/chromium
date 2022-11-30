// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_switch_throttle_observer.h"

#include "ash/constants/ash_switches.h"

namespace arc {

ArcSwitchThrottleObserver::ArcSwitchThrottleObserver()
    : ThrottleObserver("ArcSwitch") {}

void ArcSwitchThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);
  // In case switch below is set, then this observer is always active and once
  // its level is CRITICAL then this forces overall level resolution is CRITICAL
  // regardless of state of other observers. So we always set throttling as
  // CPU_RESTRICTION_FOREGROUND in the last case, that means no CPU restriction
  // happens.
  SetActive(ash::switches::IsArcCpuRestrictionDisabled());
}

}  // namespace arc
