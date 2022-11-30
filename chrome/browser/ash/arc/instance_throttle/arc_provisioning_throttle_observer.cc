// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_provisioning_throttle_observer.h"

#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace arc {

ArcProvisioningThrottleObserver::ArcProvisioningThrottleObserver()
    : ThrottleObserver("ArcIsProvisioning") {}

void ArcProvisioningThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  auto* session_manager = ArcSessionManager::Get();
  DCHECK(session_manager);
  session_manager->AddObserver(this);
  ThrottleObserver::StartObserving(context, callback);
}

void ArcProvisioningThrottleObserver::StopObserving() {
  ThrottleObserver::StopObserving();

  auto* session_manager = ArcSessionManager::Get();
  DCHECK(session_manager);
  session_manager->RemoveObserver(this);
}

// This is called when ARC starts booting. At this time we check provisioning
// status and in case of first ARC run we set this lock active until device
// is fully provisioned and |OnArcInitialStart| is called. Note, this could
// be called multiple times for the same session and provisioning status might
// be different. Last one mainly applies to some tests that do re-provisioning
// in the same session. However, rare real use case for re-provisioning is also
// possible.
void ArcProvisioningThrottleObserver::OnArcStarted() {
  SetActive(!IsArcProvisioned(Profile::FromBrowserContext(context())));
}

// Comment for |OnArcInitialStart| applies here as well but for ARC restarting
// case.
void ArcProvisioningThrottleObserver::OnArcSessionRestarting() {
  SetActive(!IsArcProvisioned(Profile::FromBrowserContext(context())));
}

// Only called when provisioning is done for first-time ARC usage. In case
// |OnArcStarted| or |OnArcSessionRestarting| is called for ARC unproviosioned
// this one must be called.
void ArcProvisioningThrottleObserver::OnArcInitialStart() {
  SetActive(false);
}

}  // namespace arc
