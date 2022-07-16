// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_BOOT_PHASE_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_BOOT_PHASE_THROTTLE_OBSERVER_H_

#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ash/throttle_observer.h"
#include "chrome/browser/sessions/session_restore_observer.h"

namespace content {
class BrowserContext;
}

namespace arc {

// This class observes phases of ARC boot and unthrottles the container
// when ARC is booting or restarting.
class ArcBootPhaseThrottleObserver : public ash::ThrottleObserver,
                                     public ArcSessionManagerObserver,
                                     public ArcBootPhaseMonitorBridge::Observer,
                                     public SessionRestoreObserver {
 public:
  ArcBootPhaseThrottleObserver();

  ArcBootPhaseThrottleObserver(const ArcBootPhaseThrottleObserver&) = delete;
  ArcBootPhaseThrottleObserver& operator=(const ArcBootPhaseThrottleObserver&) =
      delete;

  ~ArcBootPhaseThrottleObserver() override = default;

  // ash::ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;
  void StopObserving() override;

  // ArcSessionManagerObserver:
  void OnArcStarted() override;
  void OnArcInitialStart() override;
  void OnArcSessionRestarting() override;

  // ArcBootPhaseMonitorBridge::Observer:
  void OnBootCompleted() override;

  // SessionRestoreObserver:
  void OnSessionRestoreStartedLoadingTabs() override;
  void OnSessionRestoreFinishedLoadingTabs() override;

 private:
  // Enables lock if ARC is booting unless session restore is currently in
  // progress. If ARC was started for opt-in or by enterprise policy, always
  // enable since in these cases ARC should always be unthrottled during boot.
  void MaybeSetActive();

  ArcBootPhaseMonitorBridge* boot_phase_monitor_ = nullptr;
  bool session_restore_loading_ = false;
  bool arc_is_booting_ = false;
};

}  // namespace arc
#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_BOOT_PHASE_THROTTLE_OBSERVER_H_
