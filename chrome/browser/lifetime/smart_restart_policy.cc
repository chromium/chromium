// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/smart_restart_policy.h"

#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace smart_restart {

namespace {

// Returns true if the specific requirements for the trigger are met.
bool IsTriggerConditionMet(const RestartabilityState& state,
                           TriggerType trigger) {
  switch (trigger) {
    case TriggerType::kZeroWindow:
      return state.total_browser_count_is_zero;
    case TriggerType::kLockScreen:
      // Lock screen trigger implementation is pending for future phases.
      return false;
  }
  NOTREACHED();
}

}  // namespace

// static
bool SmartRestartPolicy::ShouldRestart(const RestartabilityState& state,
                                       TriggerType trigger) {
  if (!IsTriggerConditionMet(state, trigger)) {
    return false;
  }

  return !state.HasAnyActiveBlockers();
}

}  // namespace smart_restart
