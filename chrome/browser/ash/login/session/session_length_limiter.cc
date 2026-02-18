// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/session/session_length_limiter.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {

namespace {

// The minimum session time limit that can be set.
const int kSessionLengthLimitMinMs = 30 * 1000; // 30 seconds.

// The maximum session time limit that can be set.
const int kSessionLengthLimitMaxMs = 24 * 60 * 60 * 1000; // 24 hours.

}  // namespace

// static
void SessionLengthLimiter::RegisterPrefs(PrefRegistrySimple* registry) {
  session_manager::SessionStartTimeTracker::RegisterPrefs(registry);
  registry->RegisterInt64Pref(prefs::kSessionStartTime, 0);
  registry->RegisterIntegerPref(prefs::kSessionLengthLimit, 0);
}

SessionLengthLimiter::SessionLengthLimiter(
    PrefService* local_state,
    base::Clock* clock,
    session_manager::SessionManager* session_manager,
    bool browser_restarted)
    : clock_(CHECK_DEREF(clock)),
      session_manager_(CHECK_DEREF(session_manager)),
      session_start_time_tracker_(
          std::make_unique<session_manager::SessionStartTimeTracker>(
              local_state,
              clock,
              browser_restarted)) {
  CHECK(local_state);
  DCHECK(thread_checker_.CalledOnValidThread());

  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      prefs::kSessionLengthLimit,
      base::BindRepeating(&SessionLengthLimiter::UpdateLimit,
                          base::Unretained(this)));

  UpdateLimit();

  observation_.Observe(session_start_time_tracker_.get());
}

SessionLengthLimiter::~SessionLengthLimiter() = default;

base::TimeDelta SessionLengthLimiter::GetSessionDuration() const {
  base::Time session_start_time = session_start_time_tracker_->GetStartTime();
  if (session_start_time.is_null()) {
    return base::TimeDelta();
  }

  return clock_->Now() - session_start_time;
}

void SessionLengthLimiter::OnSessionStartTimeUpdated() {
  UpdateLimit();
}

base::AutoReset<raw_ref<base::Clock>> SessionLengthLimiter::SetClockForTesting(
    base::Clock* clock) {
  base::AutoReset resetter(&clock_, CHECK_DEREF(clock));
  return resetter;
}

void SessionLengthLimiter::UpdateLimit() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Stop any currently running timer.
  if (timer_)
    timer_->Stop();
  timer_.reset();

  // If instructed to wait for initial user activity and no user activity has
  // occurred yet, do not start a timer.
  base::Time session_start_time = session_start_time_tracker_->GetStartTime();
  if (session_start_time.is_null()) {
    return;
  }

  // If no session length limit is set, do not start a timer.
  const PrefService::Preference* session_length_limit_pref =
      pref_change_registrar_.prefs()->
          FindPreference(prefs::kSessionLengthLimit);
  if (session_length_limit_pref->IsDefaultValue() ||
      !session_length_limit_pref->GetValue()->is_int()) {
    return;
  }

  // Clamp the session length limit to the valid range.
  const base::TimeDelta session_length_limit = base::Milliseconds(
      std::clamp(session_length_limit_pref->GetValue()->GetInt(),
                  kSessionLengthLimitMinMs, kSessionLengthLimitMaxMs));

  // Calculate the session stop time.
  const base::Time session_stop_time =
      session_start_time + session_length_limit;

  // Log out the user immediately if the session length limit has been reached
  // or exceeded.
  if (session_stop_time <= clock_->Now()) {
    session_manager_->RequestSignOut();
    return;
  }

  // Set a timer to log out the user when the session length limit is reached.
  timer_ = std::make_unique<base::WallClockTimer>(&clock_.get(),
                                                  /*tick_clock=*/nullptr);
  timer_->Start(FROM_HERE, session_stop_time, &session_manager_.get(),
                &session_manager::SessionManager::RequestSignOut);
}

}  // namespace ash
