// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/smart_restart_policy.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/app_controller_mac.h"
#endif

namespace smart_restart {

// static
ExecutionOutcome SmartRestartPolicy::ShouldRestart(
    const RestartabilityState& state) {
  if (!state.total_browser_count_is_zero) {
    return ExecutionOutcome::kBlockedByPolicy;
  }

  if (state.HasAnyActiveBlockers()) {
    return ExecutionOutcome::kBlockedByPolicy;
  }

  return ExecutionOutcome::kExecuted;
}

// static
ExtendedExecutionOutcome SmartRestartPolicy::ShouldRestart(
    const ExtendedRestartabilityState& state,
    int tab_threshold,
    int disruption_threshold) {
  // Check tab threshold. Reject if above threshold (unless threshold is -1,
  // which means no limit).
  if (tab_threshold >= 0 && state.total_tab_count > tab_threshold) {
    return ExtendedExecutionOutcome::kBlockedByTabThreshold;
  }

  // Check disruption threshold. Reject if above threshold.
  if (std::to_underlying(state.max_disruption_level) > disruption_threshold) {
    return ExtendedExecutionOutcome::kBlockedByDisruptionLevel;
  }

  return ExtendedExecutionOutcome::kExecuted;
}

// static
ExtendedExecutionOutcome SmartRestartPolicy::CanLockScreenRestartProceed(
    const ExtendedRestartabilityState& state) {
  // Ensure the browser isn't in a delicate startup or profile-picking state.
  if (StartupBrowserCreator::InSynchronousProfileLaunch()) {
    return ExtendedExecutionOutcome::kBlockedByPolicy;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  if (ProfilePicker::IsOpen()) {
    return ExtendedExecutionOutcome::kBlockedByPolicy;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
  if (app_controller_mac::IsOpeningNewWindow()) {
    return ExtendedExecutionOutcome::kBlockedByPolicy;
  }
#endif  // BUILDFLAG(IS_MAC)

  int tab_threshold = features::kSmartRestartLockScreenTabThreshold.Get();
  int disruption_threshold =
      features::kSmartRestartLockScreenDisruptionThreshold.Get();

  return ShouldRestart(state, tab_threshold, disruption_threshold);
}

#if BUILDFLAG(IS_MAC)
// static
bool SmartRestartPolicy::CanZeroWindowRestartProceed() {
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
  return ShouldRestart(state) == ExecutionOutcome::kExecuted;
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace smart_restart
