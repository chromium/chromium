// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SMART_RESTART_POLICY_H_
#define CHROME_BROWSER_LIFETIME_SMART_RESTART_POLICY_H_

#include "chrome/browser/lifetime/restartability_monitor.h"

namespace smart_restart {

// Defines the types of triggers that can initiate a smart restart.
enum class TriggerType {
  kZeroWindow,
  kLockScreen,
};

// Responsible for deciding whether a "Smart Restart" should proceed based on
// the current browser state and user security profile.
class SmartRestartPolicy {
 public:
  SmartRestartPolicy() = delete;
  SmartRestartPolicy(const SmartRestartPolicy&) = delete;
  SmartRestartPolicy& operator=(const SmartRestartPolicy&) = delete;

  // Returns true if a restart should proceed for the given state and trigger.
  static bool ShouldRestart(const RestartabilityState& state,
                            TriggerType trigger);
};

}  // namespace smart_restart

#endif  // CHROME_BROWSER_LIFETIME_SMART_RESTART_POLICY_H_
