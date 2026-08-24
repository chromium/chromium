// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SCHEDULED_RESTART_MANAGER_H_
#define CHROME_BROWSER_LIFETIME_SCHEDULED_RESTART_MANAGER_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

class PrefRegistrySimple;

namespace features {

BASE_DECLARE_FEATURE(kScheduledRestart);

extern const base::FeatureParam<base::TimeDelta>
    kScheduledRestartFirstNudgeDelay;
extern const base::FeatureParam<base::TimeDelta> kScheduledRestartNudgeCooldown;
extern const base::FeatureParam<base::TimeDelta> kScheduledRestartIdleThreshold;
extern const base::FeatureParam<std::string> kScheduledRestartLullWindows;

}  // namespace features

namespace scheduled_restart {

// Manages scheduled browser restarts after software updates.
class ScheduledRestartManager {
 public:
  ScheduledRestartManager() = delete;
  ScheduledRestartManager(const ScheduledRestartManager&) = delete;
  ScheduledRestartManager& operator=(const ScheduledRestartManager&) = delete;

  // Registers Local State preferences used by ScheduledRestartManager.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
};

}  // namespace scheduled_restart

#endif  // CHROME_BROWSER_LIFETIME_SCHEDULED_RESTART_MANAGER_H_
