// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SMART_RESTART_MANAGER_H_
#define CHROME_BROWSER_LIFETIME_SMART_RESTART_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
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
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ExecutionOutcome {
    kExecuted = 0,         // chrome::AttemptRestartWithMode was called.
    kCancelledByUser = 1,  // User opened a window before the timer fired.
    kBlockedByPolicy = 2,  // Policy check failed after the timer fired.
    kMaxValue = kBlockedByPolicy,
  };

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

 private:
  // Starts the relaunch attempt timer if any trigger conditions are met.
  void MaybeStartRestartTimer();

  // Evaluates the current state and potentially triggers a restart.
  void MaybeExecuteSmartRestart();

#if BUILDFLAG(IS_MAC)
  // macOS-specific: Starts the timer for the Zero Window trigger.
  void MaybeStartZeroWindowTimer();

  // macOS-specific: Executes the restart for the Zero Window trigger.
  void MaybeExecuteZeroWindowRestart();

  // macOS-specific: Returns true if all conditions (window state and safety
  // policy) are met to allow a Zero-Window restart to proceed.
  bool CanZeroWindowRestartProceed();
#endif  // BUILDFLAG(IS_MAC)

  const raw_ptr<UpgradeDetector> upgrade_detector_;

  base::OneShotTimer restart_timer_;

  base::ScopedObservation<UpgradeDetector, UpgradeObserver>
      upgrade_detector_observation_{this};

#if BUILDFLAG(IS_MAC)
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
#endif  // BUILDFLAG(IS_MAC)
};

}  // namespace smart_restart

#endif  // CHROME_BROWSER_LIFETIME_SMART_RESTART_MANAGER_H_
