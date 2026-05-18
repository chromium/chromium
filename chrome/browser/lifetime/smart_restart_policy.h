// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SMART_RESTART_POLICY_H_
#define CHROME_BROWSER_LIFETIME_SMART_RESTART_POLICY_H_

#include "chrome/browser/lifetime/restartability_monitor.h"

namespace smart_restart {

// Outcomes for a Zero Window smart restart attempt.
enum class ExecutionOutcome {
  kExecuted = 0,         // chrome::AttemptRestartWithMode was called.
  kCancelledByUser = 1,  // User opened a window before the timer fired.
  kBlockedByPolicy = 2,  // Policy check failed after the timer fired.
  kMaxValue = kBlockedByPolicy,
};

// Outcomes for an extended smart restart attempt.
//
// LINT.IfChange(ExtendedExecutionOutcome)
enum class ExtendedExecutionOutcome {
  kExecuted = 0,               // chrome::AttemptRestartWithMode was called.
  kCancelledByUser = 1,        // User breaks the trigger (e.g. Unlocks screen).
  kBlockedByPolicy = 2,        // Profile picker or startup
  kBlockedByTabThreshold = 3,  // Too many tabs
  kBlockedByDisruptionLevel = 4,  // High disruption
  kMaxValue = kBlockedByDisruptionLevel,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/session/enums.xml:SmartRestartExtendedExecutionOutcome)

// Responsible for deciding whether a "Smart Restart" should proceed based on
// the current browser state and user security profile.
class SmartRestartPolicy {
 public:
  SmartRestartPolicy() = delete;
  SmartRestartPolicy(const SmartRestartPolicy&) = delete;
  SmartRestartPolicy& operator=(const SmartRestartPolicy&) = delete;

  // Returns the execution outcome based on the baseline state for Zero Window.
  static ExecutionOutcome ShouldRestart(const RestartabilityState& state);

  // Returns the execution outcome based on the high-fidelity
  // extended state and provided thresholds.
  static ExtendedExecutionOutcome ShouldRestart(
      const ExtendedRestartabilityState& state,
      int tab_threshold,
      int disruption_threshold);

  // Checks if Lock Screen restart can proceed based on policy.
  static ExtendedExecutionOutcome CanLockScreenRestartProceed(
      const ExtendedRestartabilityState& state);

#if BUILDFLAG(IS_MAC)
  // Checks if Zero Window restart can proceed based on policy.
  static bool CanZeroWindowRestartProceed();
#endif
};

}  // namespace smart_restart

#endif  // CHROME_BROWSER_LIFETIME_SMART_RESTART_POLICY_H_
