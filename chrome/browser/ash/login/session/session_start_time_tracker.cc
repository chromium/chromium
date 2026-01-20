// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/session/session_start_time_tracker.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/clock.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace session_manager {
namespace {

bool ShouldSessionLimitWaitForInitialUserActivity(PrefService* local_state) {
  // `is_demo_mode_and_count_from_session_start` is always false if the device
  // is not in
  // demo mode.
  bool is_demo_mode_and_count_from_session_start =
      ash::demo_mode::ForceSessionLengthCountFromSessionStarts();
  return local_state->GetBoolean(prefs::kSessionWaitForInitialUserActivity) &&
         !is_demo_mode_and_count_from_session_start;
}

}  // namespace

SessionStartTimeTracker::SessionStartTimeTracker(PrefService* local_state,
                                                 const base::Clock* clock,
                                                 bool browser_restarted)
    : local_state_(CHECK_DEREF(local_state)), clock_(CHECK_DEREF(clock)) {
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      prefs::kSessionWaitForInitialUserActivity,
      base::BindRepeating(&SessionStartTimeTracker::UpdateSessionStartTime,
                          base::Unretained(this)));

  // If this is a browser restart after a crash, try to restore the session
  // start time and the boolean indicating user activity from local state. If
  // this is not a browser restart after a crash or the attempt to restore
  // fails, set  the session start time to the current time and clear the
  // boolean indicating user activity.
  if (!browser_restarted || !RestoreStateAfterCrash()) {
    local_state_->ClearPref(prefs::kSessionUserActivitySeen);
    UpdateSessionStartTime();
  }

  // TODO(crbug.com/473653626): Replce with delegate with callback.
  // Also revisit the logic of user_activity_seen_. It looks like there are
  // edge cases:
  // - what to do if kSessionWaitForInitialUserActivity is changed
  //   from false to true.
  if (!user_activity_seen_) {
    ui::UserActivityDetector::Get()->AddObserver(this);
  }
}

SessionStartTimeTracker::~SessionStartTimeTracker() {
  if (!user_activity_seen_) {
    ui::UserActivityDetector::Get()->RemoveObserver(this);
  }
}

// static
void SessionStartTimeTracker::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kSessionUserActivitySeen, false);
  registry->RegisterBooleanPref(prefs::kSessionWaitForInitialUserActivity,
                                false);
}

void SessionStartTimeTracker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SessionStartTimeTracker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

base::Time SessionStartTimeTracker::GetStartTime() const {
  return start_time_;
}

void SessionStartTimeTracker::OnUserActivity(const ui::Event* event) {
  if (user_activity_seen_) {
    return;
  }

  ui::UserActivityDetector::Get()->RemoveObserver(this);
  user_activity_seen_ = true;

  local_state_->SetBoolean(prefs::kSessionUserActivitySeen, true);
  if (start_time_.is_null()) {
    // If instructed to wait for initial user activity and this is the first
    // activity in the session, set the session start time to the current time
    // and persist it in local state.
    start_time_ = clock_->Now();
    local_state_->SetInt64(prefs::kSessionStartTime,
                           start_time_.ToInternalValue());
  }
  local_state_->CommitPendingWrite();

  observer_list_.Notify(&Observer::OnSessionStartTimeUpdated);
}

bool SessionStartTimeTracker::RestoreStateAfterCrash() {
  const base::Time start_time = base::Time::FromInternalValue(
      local_state_->GetInt64(prefs::kSessionStartTime));
  if (start_time.is_null() || start_time >= clock_->Now()) {
    return false;
  }

  start_time_ = start_time;
  user_activity_seen_ =
      local_state_->GetBoolean(prefs::kSessionUserActivitySeen);

  return true;
}

void SessionStartTimeTracker::UpdateSessionStartTime() {
  if (user_activity_seen_) {
    return;
  }

  if (ShouldSessionLimitWaitForInitialUserActivity(&local_state_.get())) {
    start_time_ = base::Time();
    local_state_->ClearPref(prefs::kSessionStartTime);
  } else {
    start_time_ = clock_->Now();
    local_state_->SetInt64(prefs::kSessionStartTime,
                           start_time_.ToInternalValue());
  }
  local_state_->CommitPendingWrite();

  observer_list_.Notify(&Observer::OnSessionStartTimeUpdated);
}

}  // namespace session_manager
