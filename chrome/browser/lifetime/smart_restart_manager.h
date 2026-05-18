// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SMART_RESTART_MANAGER_H_
#define CHROME_BROWSER_LIFETIME_SMART_RESTART_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/lifetime/smart_restart_policy.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"

class BrowserCollection;
class BrowserWindowInterface;
class UpgradeDetector;

namespace smart_restart {

// Orchestrates "Smart Restarts" by listening for opportunistic system states
// and coordinating with the policy engine to execute background relaunches.
class SmartRestartManager : public BrowserCollectionObserver,
                            public UpgradeObserver {
 public:
  // Outcomes for a smart restart attempt.

  explicit SmartRestartManager(UpgradeDetector* upgrade_detector);
  SmartRestartManager(const SmartRestartManager&) = delete;
  SmartRestartManager& operator=(const SmartRestartManager&) = delete;
  ~SmartRestartManager() override;

#if BUILDFLAG(IS_MAC)
  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;
#endif  // BUILDFLAG(IS_MAC)

  // UpgradeObserver:
  void OnUpgradeRecommended() override;

  // Manual trigger for testing or simulated events.
  void SetLockedStateForTesting(bool is_locked);

 private:
  // Starts the relaunch attempt timer if any trigger conditions are met.
  void MaybeStartRestartTimer();

  // Evaluates the current state and potentially triggers a restart.
  void MaybeExecuteSmartRestart();

  // Called when the OS screen lock state changes.
  void OnLockStateChanged(bool is_locked);

  // Logic for the Lock Screen trigger.
  void MaybeStartLockScreenTimer();
  void MaybeExecuteLockScreenRestart();

#if BUILDFLAG(IS_MAC)
  // macOS-specific: Starts the timer for the Zero Window trigger.
  void MaybeStartZeroWindowTimer();

  // macOS-specific: Executes the restart for the Zero Window trigger.
  void MaybeExecuteZeroWindowRestart();
#endif  // BUILDFLAG(IS_MAC)

  // Prepares the manager for an execution attempt by setting the execution flag
  // and stopping all pending timers. If the set of objects to reset needs to
  // change, it can be done in this place.
  void PrepareForRestart();

  const raw_ptr<UpgradeDetector> upgrade_detector_;

  base::OneShotTimer zero_window_timer_;
  base::OneShotTimer lock_screen_timer_;
  bool is_executing_restart_ = false;
  bool is_locked_ = false;

  base::ScopedObservation<UpgradeDetector, UpgradeObserver>
      upgrade_detector_observation_{this};

  base::CallbackListSubscription lock_state_subscription_;

#if BUILDFLAG(IS_MAC)
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
#endif  // BUILDFLAG(IS_MAC)

  base::WeakPtrFactory<SmartRestartManager> weak_factory_{this};
};

}  // namespace smart_restart

#endif  // CHROME_BROWSER_LIFETIME_SMART_RESTART_MANAGER_H_
