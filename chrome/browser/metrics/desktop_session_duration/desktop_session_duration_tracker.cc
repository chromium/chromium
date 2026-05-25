// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/puma_histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "components/activity_reporter/activity_reporter.h"
#include "ui/events/types/event_type.h"

namespace metrics {

namespace {

DesktopSessionDurationTracker* g_desktop_session_duration_tracker_instance =
    nullptr;

// List of events which should not start a session if the instance was
// auto-launched by the OS.
constexpr ui::EventType kNonInteractiveEvents[] = {
    ui::EventType::kMouseMoved,  ui::EventType::kMouseEntered,
    ui::EventType::kMouseExited, ui::EventType::kKeyPressed,
    ui::EventType::kKeyReleased,
};

// Returns whether this instance was launched automatically by the OS as part of
// its startup.
bool IsAutoLaunchedByOs() {
#if BUILDFLAG(IS_WIN)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kStartupForegroundLaunch);
#else
  return false;
#endif  // BUILDFLAG(IS_WIN)
}

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
    // We're not considering visibility changes as explicit input events sent
    // to Chrome.
    OnUserEvent(std::nullopt);
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

void DesktopSessionDurationTracker::OnUserEvent(
    base::optional_ref<const ui::EventType> event) {
  if (!is_visible_)
    return;

  last_user_event_ = base::TimeTicks::Now();
  // This may start session.
  if (!in_session_) {
    DVLOG(4) << "Starting session due to user event";
    StartSession();
  }

  DCHECK(in_session_);

  const bool is_interactive =
      event.has_value() && !std::ranges::contains(kNonInteractiveEvents, event);

  if (is_interactive && waiting_for_first_interactive_session_) {
    waiting_for_first_interactive_session_ = false;
    interactive_session_start_time_ = base::TimeTicks::Now();
  }
}

void DesktopSessionDurationTracker::EndSessionForTesting() {
  if (!in_session_) {
    StartSession();
  }
  EndSession(base::TimeDelta());
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

DesktopSessionDurationTracker::~DesktopSessionDurationTracker() = default;

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

  // We should not wait for an interactive event if this is a user-launched
  // instance.
  if (!IsAutoLaunchedByOs()) {
    waiting_for_first_interactive_session_ = false;
  }

  // If we are not waiting for an interactive event (not OS-launched or
  // an interactive session has already started in the past)
  // `interactive_session_start_time_` should be the same as `session_start_`.
  if (!waiting_for_first_interactive_session_) {
    interactive_session_start_time_ = session_start_;
  }

  StartTimer(inactivity_timeout_);

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
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delta = now - session_start_;

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

  // This will log duration of the first interactive session for OS-launched
  // instances, or the same duration as `Session.TotalDuration` in all other
  // cases.
  if (!waiting_for_first_interactive_session_) {
    base::TimeDelta interactive_delta =
        now - interactive_session_start_time_ - time_to_discount;
    if (interactive_delta.is_negative()) {
      interactive_delta = base::TimeDelta();
    }
    UMA_HISTOGRAM_LONG_TIMES(
        "Session.TotalDuration.IgnoreNonInteractiveTimeForOSLaunchedSessions",
        interactive_delta);
  }

  // Records true each time Session.TotalDuration is supposed to be recorded
  // in a PUMA histogram. Allowing for the count to be collected.
  base::PumaHistogramBoolean(
      base::PumaType::kRc,
      "PUMA.RegionalCapabilities.Session.TotalDuration.Recorded", true);

  UMA_HISTOGRAM_CUSTOM_TIMES("Session.TotalDurationMax1Day", delta,
                             base::Milliseconds(1), base::Hours(24), 50);

  g_browser_process->activity_reporter()->ReportActive();
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
