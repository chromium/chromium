// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/scheduled_restart_manager.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace features {

// Controls the Scheduled Restart feature on Desktop platforms.
BASE_FEATURE(kScheduledRestart, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kScheduledRestartFirstNudgeDelay{
    &kScheduledRestart, "first_nudge_delay", base::Days(14)};

const base::FeatureParam<base::TimeDelta> kScheduledRestartNudgeCooldown{
    &kScheduledRestart, "nudge_cooldown", base::Days(14)};

const base::FeatureParam<base::TimeDelta> kScheduledRestartIdleThreshold{
    &kScheduledRestart, "idle_threshold", base::Seconds(300)};

const base::FeatureParam<std::string> kScheduledRestartLullWindows{
    &kScheduledRestart, "lull_windows", "11:30-12:30,15:00-17:00"};

}  // namespace features

namespace scheduled_restart {

// static
void ScheduledRestartManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // Stored timestamp of when the Scheduled Restart reminder nudge was last
  // shown to the user. Read and updated by ScheduledRestartManager in
  // downstream CLs to enforce reminder cooldowns.
  registry->RegisterTimePref(prefs::kScheduledRestartLastNudgeTime,
                             base::Time());
}

}  // namespace scheduled_restart
