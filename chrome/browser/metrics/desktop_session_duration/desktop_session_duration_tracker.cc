// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"

namespace metrics {

namespace {

DesktopSessionDurationTracker* g_desktop_session_duration_tracker_instance =
    nullptr;

}  // namespace

// static
void DesktopSessionDurationTracker::Initialize() {
  DCHECK(!g_desktop_session_duration_tracker_instance);
  g_desktop_session_duration_tracker_instance =
      new DesktopSessionDurationTracker;
}

// static
bool DesktopSessionDurationTracker::IsInitialized() {
  return g_desktop_session_duration_tracker_instance != nullptr;
}

// static
DesktopSessionDurationTracker* DesktopSessionDurationTracker::Get() {
  DCHECK(g_desktop_session_duration_tracker_instance);
  return g_desktop_session_duration_tracker_instance;
}

void DesktopSessionDurationTracker::StartTimer(base::TimeDelta duration) {
  timer_.Start(FROM_HERE, duration,
               base::BindOnce(&DesktopSessionDurationTracker::OnTimerFired,
                              weak_factory_.GetWeakPtr()));
}

void DesktopSessionDurationTracker::OnVisibilityChanged(
    bool visible,
    base::TimeDelta time_ago) {
  is_visible_ = visible;
  if (is_visible_ && !is_first_session_) {
    DCHECK(time_ago.is_zero());
    OnUserEvent();
  } else if (in_session_ && !is_audio_playing_) {
    DCHECK(!visible);
    DVLOG(4) << "Ending session due to visibility change";
    EndSession(time_ago);
  }
}

void DesktopSessionDurationTracker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DesktopSessionDurationTracker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void DesktopSessionDurationTracker::OnUserEvent() {
  if (!is_visible_)
    return;

  last_user_event_ = base::TimeTicks::Now();
  // This may start session.
  if (!in_session_) {
    DVLOG(4) << "Starting session due to user event";
    StartSession();
  }
}

// static
void DesktopSessionDurationTracker::CleanupForTesting() {
  DCHECK(g_desktop_session_duration_tracker_instance);
  delete g_desktop_session_duration_tracker_instance;
  g_desktop_session_duration_tracker_instance = nullptr;
}

void DesktopSessionDurationTracker::OnAudioStart() {
  // This may start session.
  is_audio_playing_ = true;
  if (!in_session_) {
    DVLOG(4) << "Starting session due to audio start";
    StartSession();
  }
}

void DesktopSessionDurationTracker::OnAudioEnd() {
  is_audio_playing_ = false;

  // If the timer is not running, this means that no user events happened in the
  // last 5 minutes so the session can be terminated.
  if (!timer_.IsRunning()) {
    DVLOG(4) << "Ending session due to audio ending";
    EndSession(base::TimeDelta());
  }
}

DesktopSessionDurationTracker::DesktopSessionDurationTracker()
    : session_start_(base::TimeTicks::Now()),
      last_user_event_(session_start_),
      audio_tracker_(this) {
  InitInactivityTimeout();
}

DesktopSessionDurationTracker::~DesktopSessionDurationTracker() {}

void DesktopSessionDurationTracker::OnTimerFired() {
  base::TimeDelta remaining =
      inactivity_timeout_ - (base::TimeTicks::Now() - last_user_event_);
  if (remaining.ToInternalValue() > 0) {
    StartTimer(remaining);
    return;
  }

  // No user events happened in the last 5 min. Terminate the session now.
  if (!is_audio_playing_) {
    DVLOG(4) << "Ending session after delay";
    EndSession(inactivity_timeout_);
  }
}

void DesktopSessionDurationTracker::StartSession() {
  DCHECK(!in_session_);
  in_session_ = true;
  is_first_session_ = false;
  session_start_ = base::TimeTicks::Now();
  StartTimer(inactivity_timeout_);
  ResetDefaultSearchCounter();

  for (Observer& observer : observer_list_)
    observer.OnSessionStarted(session_start_);
}

void DesktopSessionDurationTracker::EndSession(
    base::TimeDelta time_to_discount) {
  DCHECK(in_session_);
  in_session_ = false;

  // Cancel the inactivity timer, to prevent the session from ending a second
  // time when it expires.
  timer_.Stop();

  base::TimeDelta delta = base::TimeTicks::Now() - session_start_;

  // Trim any timeouts from the session length and lower bound to a session of
  // length 0.
  delta -= time_to_discount;
  if (delta.is_negative())
    delta = base::TimeDelta();

  for (Observer& observer : observer_list_)
    observer.OnSessionEnded(delta, session_start_ + delta);

  DVLOG(4) << "Logging session length of " << delta.InSeconds() << " seconds.";

  // Note: This metric is recorded separately for Android in
  // UmaSessionStats::UmaEndSession.
  UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration", delta);

  UMA_HISTOGRAM_CUSTOM_TIMES("Session.TotalDurationMax1Day", delta,
                             base::Milliseconds(1), base::Hours(24), 50);

  UMA_HISTOGRAM_COUNTS_1000("Session.TotalNewDefaultSearchEngineCount",
                            default_search_counter_);
}

void DesktopSessionDurationTracker::InitInactivityTimeout() {
  const int kDefaultInactivityTimeoutMinutes = 5;

  int timeout_minutes = kDefaultInactivityTimeoutMinutes;
  std::string param_value = base::GetFieldTrialParamValue(
      "DesktopSessionDuration", "inactivity_timeout");
  if (!param_value.empty())
    base::StringToInt(param_value, &timeout_minutes);

  inactivity_timeout_ = base::Minutes(timeout_minutes);
}

}  // namespace metrics
