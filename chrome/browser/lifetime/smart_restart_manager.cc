// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/smart_restart_manager.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "chrome/browser/lifetime/smart_restart_policy.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/idle/idle.h"

namespace smart_restart {

namespace {

constexpr int kLowTabThreshold = 5;
constexpr int kMediumTabThreshold = 20;

std::string_view GetTabCountVariant(int count) {
  if (count <= kLowTabThreshold) {
    return ".LowTab";
  }
  if (count <= kMediumTabThreshold) {
    return ".MediumTab";
  }
  return ".HighTab";
}

void RecordLockScreenExecutionMetrics(
    ExtendedExecutionOutcome outcome,
    const ExtendedRestartabilityState& state) {
  base::UmaHistogramEnumeration("Session.SmartRestart.Lock.ExecutionOutcome",
                                outcome);

  std::string_view suffix = GetTabCountVariant(state.total_tab_count);
  if (!suffix.empty()) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Session.SmartRestart.Lock.ExecutionOutcome", suffix}),
        outcome);
  }

  if (outcome == ExtendedExecutionOutcome::kBlockedByDisruptionLevel ||
      outcome == ExtendedExecutionOutcome::kBlockedByTabThreshold) {
    using Blocker = ExtendedRestartabilityState::SmartRestartBlocker;
    for (Blocker blocker : state.blockers) {
      base::UmaHistogramEnumeration(
          "Session.SmartRestart.Lock.ProtectionReason", blocker);
      if (!suffix.empty()) {
        base::UmaHistogramEnumeration(
            base::StrCat(
                {"Session.SmartRestart.Lock.ProtectionReason", suffix}),
            blocker);
      }
    }
  }
}

}  // namespace

SmartRestartManager::SmartRestartManager(UpgradeDetector* upgrade_detector)
    : upgrade_detector_(upgrade_detector) {
  upgrade_detector_observation_.Observe(upgrade_detector_);
#if BUILDFLAG(IS_MAC)
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
#endif  // BUILDFLAG(IS_MAC)

  // Exclude ChromeOS from Lock Screen trigger because calling AttemptRestart
  // on ChromeOS triggers a full system reboot when an update is pending,
  // which is too disruptive.
#if !BUILDFLAG(IS_CHROMEOS)
  lock_state_subscription_ = ui::AddScreenLockCallback(base::BindRepeating(
      &SmartRestartManager::OnLockStateChanged, base::Unretained(this)));
  if (ui::CheckIdleStateIsLocked()) {
    OnLockStateChanged(true);
  }
  const auto* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kSimulateLockScreenSmartRestart)) {
    std::string delay_str = cmd_line->GetSwitchValueASCII(
        switches::kSimulateLockScreenSmartRestart);
    int delay_secs = 60;  // Default to 1 minute delay if no value provided.
    if (!delay_str.empty()) {
      base::StringToInt(delay_str, &delay_secs);
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SmartRestartManager::SetLockedStateForTesting,
                       weak_factory_.GetWeakPtr(), true),
        base::Seconds(delay_secs));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

SmartRestartManager::~SmartRestartManager() = default;

void SmartRestartManager::OnUpgradeRecommended() {
  MaybeStartRestartTimer();
}

void SmartRestartManager::SetLockedStateForTesting(bool is_locked) {
  OnLockStateChanged(is_locked);
}

void SmartRestartManager::MaybeStartRestartTimer() {
  if (!upgrade_detector_->is_upgrade_available()) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  MaybeStartZeroWindowTimer();
#endif  // BUILDFLAG(IS_MAC)

#if !BUILDFLAG(IS_CHROMEOS)
  MaybeStartLockScreenTimer();
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

void SmartRestartManager::OnLockStateChanged(bool is_locked) {
  if (is_locked == is_locked_) {
    return;
  }
  is_locked_ = is_locked;

  if (is_locked) {
    MaybeStartRestartTimer();
    return;
  }

  if (lock_screen_timer_.IsRunning()) {
    base::TimeDelta remaining =
        lock_screen_timer_.desired_run_time() - base::TimeTicks::Now();
    base::UmaHistogramLongTimes(
        "Session.SmartRestart.Lock.RemainingTimeAtCancellation",
        std::max(base::TimeDelta(), remaining));

    base::UmaHistogramEnumeration("Session.SmartRestart.Lock.ExecutionOutcome",
                                  ExtendedExecutionOutcome::kCancelledByUser);
    lock_screen_timer_.Stop();
  }
}

void SmartRestartManager::MaybeStartLockScreenTimer() {
  if (!base::FeatureList::IsEnabled(features::kSmartRestartLockScreen)) {
    return;
  }
  if (is_locked_ && !lock_screen_timer_.IsRunning()) {
    lock_screen_timer_.Start(
        FROM_HERE, features::kSmartRestartLockScreenDelay.Get(),
        base::BindOnce(&SmartRestartManager::MaybeExecuteLockScreenRestart,
                       base::Unretained(this)));
  }
}

void SmartRestartManager::MaybeExecuteLockScreenRestart() {
  if (is_executing_restart_ || !is_locked_) {
    return;
  }

  ExtendedRestartabilityState state =
      RestartabilityMonitor::ComputeExtendedRestartabilityState();
  ExtendedExecutionOutcome outcome =
      SmartRestartPolicy::CanLockScreenRestartProceed(state);

  RecordLockScreenExecutionMetrics(outcome, state);

  if (outcome == ExtendedExecutionOutcome::kExecuted) {
    PrepareForRestart();

    base::TimeDelta gap =
        base::Time::Now() - upgrade_detector_->upgrade_detected_time();
    base::UmaHistogramCustomCounts(
        "Session.SmartRestart.Lock.TimeSinceUpgradeDetected", gap.InMinutes(),
        1, base::Days(30).InMinutes(), 50);

    // If the user has 0 tabs open restart in the background to avoid
    // unexpectedly opening a visible window when they return.
    chrome::AttemptRestartWithMode(state.total_tab_count == 0
                                       ? chrome::RelaunchMode::kBackground
                                       : chrome::RelaunchMode::kNormal);
  }
}

#if BUILDFLAG(IS_MAC)
void SmartRestartManager::OnBrowserCreated(BrowserWindowInterface* browser) {
  if (zero_window_timer_.IsRunning()) {
    // Record how much time was remaining in the grace period before
    // cancellation. This helps identify "close calls" where we almost
    // disrupted the user.
    base::TimeDelta remaining =
        zero_window_timer_.desired_run_time() - base::TimeTicks::Now();
    base::UmaHistogramLongTimes(
        "Session.SmartRestart.ZeroWindow.RemainingTimeAtCancellation",
        std::max(base::TimeDelta(), remaining));

    base::UmaHistogramEnumeration(
        "Session.SmartRestart.ZeroWindow.ExecutionOutcome",
        ExecutionOutcome::kCancelledByUser);
    zero_window_timer_.Stop();
  }
}

void SmartRestartManager::OnBrowserClosed(BrowserWindowInterface* browser) {
  MaybeStartRestartTimer();
}

void SmartRestartManager::MaybeStartZeroWindowTimer() {
  // Only start the countdown if the state is currently clean and safe.
  if (SmartRestartPolicy::CanZeroWindowRestartProceed() &&
      !zero_window_timer_.IsRunning()) {
    zero_window_timer_.Start(
        FROM_HERE, features::kSmartRestartDelay.Get(),
        base::BindOnce(&SmartRestartManager::MaybeExecuteZeroWindowRestart,
                       base::Unretained(this)));
  }
}

void SmartRestartManager::MaybeExecuteZeroWindowRestart() {
  if (is_executing_restart_) {
    return;
  }

  // Perform a final safety check before execution to handle any state changes
  // during the grace period (e.g. a download started).
  if (SmartRestartPolicy::CanZeroWindowRestartProceed()) {
    PrepareForRestart();

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
#endif  // BUILDFLAG(IS_MAC)

void SmartRestartManager::PrepareForRestart() {
  is_executing_restart_ = true;
  zero_window_timer_.Stop();
  lock_screen_timer_.Stop();
}

}  // namespace smart_restart
