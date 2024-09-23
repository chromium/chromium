// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/session/session_length_limiter.h"

#include <algorithm>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/event.h"

namespace ash {

namespace {

// The minimum session time limit that can be set.
const int kSessionLengthLimitMinMs = 30 * 1000; // 30 seconds.

// The maximum session time limit that can be set.
const int kSessionLengthLimitMaxMs = 24 * 60 * 60 * 1000; // 24 hours.

// A default delegate implementation that returns the current time and does end
// the current user's session when requested. This can be replaced with a mock
// in tests.
class SessionLengthLimiterDelegateImpl : public SessionLengthLimiter::Delegate {
 public:
  SessionLengthLimiterDelegateImpl();

  SessionLengthLimiterDelegateImpl(const SessionLengthLimiterDelegateImpl&) =
      delete;
  SessionLengthLimiterDelegateImpl& operator=(
      const SessionLengthLimiterDelegateImpl&) = delete;

  ~SessionLengthLimiterDelegateImpl() override;

  const base::Clock* GetClock() const override;
  void StopSession() override;
};

SessionLengthLimiterDelegateImpl::SessionLengthLimiterDelegateImpl() {
}

SessionLengthLimiterDelegateImpl::~SessionLengthLimiterDelegateImpl() {
}

const base::Clock* SessionLengthLimiterDelegateImpl::GetClock() const {
  return base::DefaultClock::GetInstance();
}

void SessionLengthLimiterDelegateImpl::StopSession() {
  chrome::AttemptUserExit();
}

}  // namespace

SessionLengthLimiter::Delegate::~Delegate() {
}

// static
void SessionLengthLimiter::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kSessionUserActivitySeen, false);
  registry->RegisterInt64Pref(prefs::kSessionStartTime, 0);
  registry->RegisterIntegerPref(prefs::kSessionLengthLimit, 0);
  registry->RegisterBooleanPref(prefs::kSessionWaitForInitialUserActivity,
                                false);
}

SessionLengthLimiter::SessionLengthLimiter(Delegate* delegate,
                                           bool browser_restarted)
    : delegate_(delegate ? delegate : new SessionLengthLimiterDelegateImpl),
      user_activity_seen_(false) {
  DCHECK(thread_checker_.CalledOnValidThread());

  PrefService* local_state = g_browser_process->local_state();
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      prefs::kSessionLengthLimit,
      base::BindRepeating(&SessionLengthLimiter::UpdateLimit,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSessionWaitForInitialUserActivity,
      base::BindRepeating(&SessionLengthLimiter::UpdateSessionStartTime,
                          base::Unretained(this)));

  // If this is a browser restart after a crash, try to restore the session
  // start time and the boolean indicating user activity from local state. If
  // this is not a browser restart after a crash or the attempt to restore
  // fails, set  the session start time to the current time and clear the
  // boolean indicating user activity.
  if (!browser_restarted || !RestoreStateAfterCrash()) {
    local_state->ClearPref(prefs::kSessionUserActivitySeen);
    UpdateSessionStartTime();
  }

  if (!user_activity_seen_) {
    ui::UserActivityDetector::Get()->AddObserver(this);
  }
}

SessionLengthLimiter::~SessionLengthLimiter() {
  if (!user_activity_seen_) {
    ui::UserActivityDetector::Get()->RemoveObserver(this);
  }
}

base::TimeDelta SessionLengthLimiter::GetSessionDuration() const {
  if (session_start_time_.is_null())
    return base::TimeDelta();

  return delegate_->GetClock()->Now() - session_start_time_;
}

void SessionLengthLimiter::OnUserActivity(const ui::Event* event) {
  if (user_activity_seen_)
    return;
  ui::UserActivityDetector::Get()->RemoveObserver(this);
  user_activity_seen_ = true;

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kSessionUserActivitySeen, true);
  if (session_start_time_.is_null()) {
    // If instructed to wait for initial user activity and this is the first
    // activity in the session, set the session start time to the current time
    // and persist it in local state.
    session_start_time_ = delegate_->GetClock()->Now();
    local_state->SetInt64(prefs::kSessionStartTime,
                          session_start_time_.ToInternalValue());
  }
  local_state->CommitPendingWrite();

  UpdateLimit();
}

bool SessionLengthLimiter::RestoreStateAfterCrash() {
  PrefService* local_state = g_browser_process->local_state();
  const base::Time session_start_time = base::Time::FromInternalValue(
      local_state->GetInt64(prefs::kSessionStartTime));
  if (session_start_time.is_null() ||
      session_start_time >= delegate_->GetClock()->Now()) {
    return false;
  }

  session_start_time_ = session_start_time;
  user_activity_seen_ =
      local_state->GetBoolean(prefs::kSessionUserActivitySeen);

  UpdateLimit();
  return true;
}

void SessionLengthLimiter::UpdateSessionStartTime() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (user_activity_seen_)
    return;

  PrefService* local_state = g_browser_process->local_state();
  if (local_state->GetBoolean(prefs::kSessionWaitForInitialUserActivity)) {
    session_start_time_ = base::Time();
    local_state->ClearPref(prefs::kSessionStartTime);
  } else {
    session_start_time_ = delegate_->GetClock()->Now();
    local_state->SetInt64(prefs::kSessionStartTime,
                          session_start_time_.ToInternalValue());
  }
  local_state->CommitPendingWrite();

  UpdateLimit();
}

void SessionLengthLimiter::UpdateLimit() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Stop any currently running timer.
  if (timer_)
    timer_->Stop();
  timer_.reset();

  // If instructed to wait for initial user activity and no user activity has
  // occurred yet, do not start a timer.
  if (session_start_time_.is_null())
    return;

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
      session_start_time_ + session_length_limit;

  // Log out the user immediately if the session length limit has been reached
  // or exceeded.
  if (session_stop_time <= delegate_->GetClock()->Now()) {
    delegate_->StopSession();
    return;
  }

  // Set a timer to log out the user when the session length limit is reached.
  timer_ = std::make_unique<base::WallClockTimer>(
      delegate_->GetClock() /*clock*/, nullptr /*tick_clock*/);
  timer_->Start(FROM_HERE, session_stop_time, delegate_.get(),
                &SessionLengthLimiter::Delegate::StopSession);
}

}  // namespace ash
