// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arcvm_kiosk_mode_throttle_observer.h"

#include "components/user_manager/user_manager.h"

namespace arc {

ArcvmKioskModeThrottleObserver::ArcvmKioskModeThrottleObserver()
    : ThrottleObserver("ArcvmKioskMode") {}

void ArcvmKioskModeThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);
  DCHECK(user_manager::UserManager::IsInitialized());
  if (user_manager::UserManager::Get()->IsLoggedInAsKioskArcvmApp()) {
    SetEnforced(true);
    SetActive(true);
  } else {
    SetEnforced(false);
    SetActive(false);
  }
}

void ArcvmKioskModeThrottleObserver::StopObserving() {
  ThrottleObserver::StopObserving();
  SetEnforced(false);
  SetActive(false);
}

}  // namespace arc
