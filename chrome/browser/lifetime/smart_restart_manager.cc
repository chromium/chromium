// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/smart_restart_manager.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "chrome/browser/lifetime/smart_restart_policy.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_features.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/app_controller_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace smart_restart {

SmartRestartManager::SmartRestartManager(UpgradeDetector* upgrade_detector)
    : upgrade_detector_(upgrade_detector) {
  upgrade_detector_observation_.Observe(upgrade_detector_);
#if BUILDFLAG(IS_MAC)
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
#endif  // BUILDFLAG(IS_MAC)
}

SmartRestartManager::~SmartRestartManager() = default;

void SmartRestartManager::OnUpgradeRecommended() {
  MaybeStartRestartTimer();
}

void SmartRestartManager::MaybeStartRestartTimer() {
  if (!upgrade_detector_->is_upgrade_available()) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  MaybeStartZeroWindowTimer();
#endif  // BUILDFLAG(IS_MAC)
}

void SmartRestartManager::MaybeExecuteSmartRestart() {
  if (!upgrade_detector_->is_upgrade_available()) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  MaybeExecuteZeroWindowRestart();
#endif  // BUILDFLAG(IS_MAC)
}

#if BUILDFLAG(IS_MAC)
void SmartRestartManager::OnBrowserCreated(BrowserWindowInterface* browser) {
  if (restart_timer_.IsRunning()) {
    // Record how much time was remaining in the grace period before
    // cancellation. This helps identify "close calls" where we almost
    // disrupted the user.
    base::TimeDelta remaining =
        restart_timer_.desired_run_time() - base::TimeTicks::Now();
    base::UmaHistogramLongTimes(
        "Session.SmartRestart.ZeroWindow.RemainingTimeAtCancellation",
        std::max(base::TimeDelta(), remaining));

    base::UmaHistogramEnumeration(
        "Session.SmartRestart.ZeroWindow.ExecutionOutcome",
        ExecutionOutcome::kCancelledByUser);
    restart_timer_.Stop();
  }
}

void SmartRestartManager::OnBrowserClosed(BrowserWindowInterface* browser) {
  MaybeStartRestartTimer();
}

void SmartRestartManager::MaybeStartZeroWindowTimer() {
  // Only start the countdown if the state is currently clean and safe.
  if (CanZeroWindowRestartProceed() && !restart_timer_.IsRunning()) {
    restart_timer_.Start(
        FROM_HERE, features::kSmartRestartDelay.Get(),
        base::BindOnce(&SmartRestartManager::MaybeExecuteSmartRestart,
                       base::Unretained(this)));
  }
}

void SmartRestartManager::MaybeExecuteZeroWindowRestart() {
  // Perform a final safety check before execution to handle any state changes
  // during the grace period (e.g. a download started).
  if (CanZeroWindowRestartProceed()) {
    // Record how long the upgrade had been waiting before we finally captured
    // this zero-window opportunity. Use a 7-day range to capture multi-day
    // gaps between update detection and patching.
    base::TimeDelta gap =
        base::Time::Now() - upgrade_detector_->upgrade_detected_time();
    base::UmaHistogramCustomTimes(
        "Session.SmartRestart.ZeroWindow.TimeSinceUpgradeDetected", gap,
        base::Seconds(1), base::Days(7), 50);

    base::UmaHistogramEnumeration(
        "Session.SmartRestart.ZeroWindow.ExecutionOutcome",
        ExecutionOutcome::kExecuted);
    chrome::AttemptRestartWithMode(chrome::RelaunchMode::kBackground);
  } else {
    base::UmaHistogramEnumeration(
        "Session.SmartRestart.ZeroWindow.ExecutionOutcome",
        ExecutionOutcome::kBlockedByPolicy);
  }
}

bool SmartRestartManager::CanZeroWindowRestartProceed() {
  // 1. Ensure the browser isn't in a delicate startup or profile-picking state.
  if (StartupBrowserCreator::InSynchronousProfileLaunch() ||
      ProfilePicker::IsOpen()) {
    return false;
  }

  if (app_controller_mac::IsOpeningNewWindow()) {
    return false;
  }

  // 2. Evaluates current blockers (Downloads, Media) and ensures the browser
  // is windowless via the total_browser_count_is_zero check.
  RestartabilityState state = RestartabilityMonitor::ComputeCurrentState();
  return SmartRestartPolicy::ShouldRestart(state, TriggerType::kZeroWindow);
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace smart_restart
