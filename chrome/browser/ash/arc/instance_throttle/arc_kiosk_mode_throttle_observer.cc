// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_kiosk_mode_throttle_observer.h"

#include "ash/components/arc/arc_util.h"
#include "components/user_manager/user_manager.h"

namespace arc {

ArcKioskModeThrottleObserver::ArcKioskModeThrottleObserver()
    : ThrottleObserver("ArcKioskMode") {}

void ArcKioskModeThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);
  DCHECK(user_manager::UserManager::IsInitialized());
  SetActive(IsArcKioskMode());
}

void ArcKioskModeThrottleObserver::StopObserving() {
  ThrottleObserver::StopObserving();
}

}  // namespace arc
