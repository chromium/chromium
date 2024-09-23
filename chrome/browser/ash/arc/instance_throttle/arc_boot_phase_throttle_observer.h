// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_BOOT_PHASE_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_BOOT_PHASE_THROTTLE_OBSERVER_H_

#include <optional>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/connection_holder.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/sessions/session_restore_observer.h"
#include "chromeos/ash/components/throttle/throttle_observer.h"

namespace content {
class BrowserContext;
}

namespace arc {

constexpr char kArcBootPhaseThrottleObserverName[] = "ArcIsBooting";

// This class observes phases of ARC boot and unthrottles the container
// when ARC is booting or restarting.
class ArcBootPhaseThrottleObserver
    : public ash::ThrottleObserver,
      public ArcSessionManagerObserver,
      public SessionRestoreObserver,
      public arc::ConnectionObserver<arc::mojom::AppInstance>,
      public arc::ConnectionObserver<arc::mojom::IntentHelperInstance> {
 public:
  ArcBootPhaseThrottleObserver();

  ArcBootPhaseThrottleObserver(const ArcBootPhaseThrottleObserver&) = delete;
  ArcBootPhaseThrottleObserver& operator=(const ArcBootPhaseThrottleObserver&) =
      delete;

  ~ArcBootPhaseThrottleObserver() override;

  // ash::ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;
  void StopObserving() override;

  // ArcSessionManagerObserver:
  void OnArcStarted() override;
  void OnArcInitialStart() override;
  void OnArcSessionRestarting() override;

  // SessionRestoreObserver:
  void OnSessionRestoreStartedLoadingTabs() override;
  void OnSessionRestoreFinishedLoadingTabs() override;

  // arc::ConnectionObserver<arc::mojom::AppInstance> overrides.
  // arc::ConnectionObserver<arc::mojom::IntentHelperInstance> overrides.
  void OnConnectionReady() override;

  // If nullopt, ARC hasn't been started yet. Otherwise, true means ARC is
  // booting, and false means ARC has already booted.
  const std::optional<bool>& arc_is_booting() const { return arc_is_booting_; }

  static const base::TimeDelta& GetThrottleDelayForTesting();

 private:
  void ThrottleArc();

  // Enables lock if ARC is booting unless session restore is currently in
  // progress. If ARC was started for opt-in or by enterprise policy, always
  // enable since in these cases ARC should always be unthrottled during boot.
  void MaybeSetActive();

  bool session_restore_loading_ = false;
  // This is set when one of the ArcSessionManagerObserver functions is called.
  std::optional<bool> arc_is_booting_;

  base::WeakPtrFactory<ArcBootPhaseThrottleObserver> weak_ptr_factory_{this};
};

}  // namespace arc
#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_BOOT_PHASE_THROTTLE_OBSERVER_H_
