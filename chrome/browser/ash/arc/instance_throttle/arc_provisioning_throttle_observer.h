// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_PROVISIONING_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_PROVISIONING_THROTTLE_OBSERVER_H_

#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chromeos/ash/components/throttle/throttle_observer.h"

namespace content {
class BrowserContext;
}

namespace arc {

// This class observes ARC provisioning state and keeps ARC unthrottled until
// provisioning is done.
class ArcProvisioningThrottleObserver : public ash::ThrottleObserver,
                                        public ArcSessionManagerObserver {
 public:
  ArcProvisioningThrottleObserver();

  ArcProvisioningThrottleObserver(const ArcProvisioningThrottleObserver&) =
      delete;
  ArcProvisioningThrottleObserver& operator=(
      const ArcProvisioningThrottleObserver&) = delete;

  ~ArcProvisioningThrottleObserver() override = default;

  // ash::ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;
  void StopObserving() override;

  // ArcSessionManagerObserver:
  void OnArcStarted() override;
  void OnArcSessionRestarting() override;
  void OnArcInitialStart() override;
};

}  // namespace arc
#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_PROVISIONING_THROTTLE_OBSERVER_H_
