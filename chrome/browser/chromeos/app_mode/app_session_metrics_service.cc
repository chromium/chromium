// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_session_metrics_service.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace chromeos {

namespace {

bool IsRestoredSession() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Return true for a kiosk session restored after crash.
  // The kiosk session gets restored to a state that was prior to crash:
  // * no --login-manager command line flag, since no login screen is shown
  //   in the middle of a kiosk session.
  // * --login-user command line flag is present, because the session is
  //   re-started in the middle and kiosk profile is already logged in.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             ash::switches::kLoginManager) &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             ash::switches::kLoginUser);
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

const char kKioskSessionStateHistogram[] = "Kiosk.SessionState";
const char kKioskSessionCountPerDayHistogram[] = "Kiosk.Session.CountPerDay";
const char kKioskSessionDurationNormalHistogram[] =
    "Kiosk.SessionDuration.Normal";
const char kKioskSessionDurationInDaysNormalHistogram[] =
    "Kiosk.SessionDurationInDays.Normal";
const char kKioskSessionDurationCrashedHistogram[] =
    "Kiosk.SessionDuration.Crashed";
const char kKioskSessionDurationInDaysCrashedHistogram[] =
    "Kiosk.SessionDurationInDays.Crashed";
const char kKioskSessionLastDayList[] = "last-day-sessions";
const char kKioskSessionStartTime[] = "session-start-time";

const int kKioskHistogramBucketCount = 100;
const base::TimeDelta kKioskSessionDurationHistogramLimit = base::Days(1);

AppSessionMetricsService::AppSessionMetricsService(PrefService* prefs)
    : prefs_(prefs) {}

void AppSessionMetricsService::RecordKioskSessionStarted() {
  RecordKioskSessionStarted(KioskSessionState::kStarted);
}

void AppSessionMetricsService::RecordKioskSessionWebStarted() {
  RecordKioskSessionStarted(KioskSessionState::kWebStarted);
}

void AppSessionMetricsService::RecordKioskSessionStopped() {
  if (!IsKioskSessionRunning())
    return;
  RecordKioskSessionState(KioskSessionState::kStopped);
  RecordKioskSessionDuration(kKioskSessionDurationNormalHistogram,
                             kKioskSessionDurationInDaysNormalHistogram);
}

void AppSessionMetricsService::RecordKioskSessionCrashed() {
  if (!IsKioskSessionRunning())
    return;
  RecordKioskSessionState(KioskSessionState::kCrashed);
  RecordKioskSessionDuration(kKioskSessionDurationCrashedHistogram,
                             kKioskSessionDurationInDaysCrashedHistogram);
}

void AppSessionMetricsService::RecordKioskSessionPluginCrashed() {
  RecordKioskSessionState(KioskSessionState::kPluginCrashed);
  RecordKioskSessionDuration(kKioskSessionDurationCrashedHistogram,
                             kKioskSessionDurationInDaysCrashedHistogram);
}

void AppSessionMetricsService::RecordKioskSessionPluginHung() {
  RecordKioskSessionState(KioskSessionState::kPluginHung);
  RecordKioskSessionDuration(kKioskSessionDurationCrashedHistogram,
                             kKioskSessionDurationInDaysCrashedHistogram);
}

bool AppSessionMetricsService::IsKioskSessionRunning() const {
  return !start_time_.is_null();
}

void AppSessionMetricsService::RecordKioskSessionStarted(
    KioskSessionState started_state) {
  RecordPreviousKioskSessionCrashIfAny();
  if (IsRestoredSession()) {
    RecordKioskSessionState(KioskSessionState::kRestored);
  } else {
    RecordKioskSessionState(started_state);
  }
  RecordKioskSessionCountPerDay();
}

void AppSessionMetricsService::RecordKioskSessionState(
    KioskSessionState state) const {
  base::UmaHistogramEnumeration(kKioskSessionStateHistogram, state);
}

void AppSessionMetricsService::RecordKioskSessionCountPerDay() {
  base::UmaHistogramCounts100(kKioskSessionCountPerDayHistogram,
                              RetrieveLastDaySessionCount(base::Time::Now()));
}

void AppSessionMetricsService::RecordKioskSessionDuration(
    const std::string& kiosk_session_duration_histogram,
    const std::string& kiosk_session_duration_in_days_histogram) {
  if (!IsKioskSessionRunning())
    return;
  base::TimeDelta duration = base::Time::Now() - start_time_;
  if (duration >= kKioskSessionDurationHistogramLimit) {
    base::UmaHistogramCounts100(kiosk_session_duration_in_days_histogram,
                                std::min(100, duration.InDays()));
    duration = kKioskSessionDurationHistogramLimit;
  }
  base::UmaHistogramCustomTimes(
      kiosk_session_duration_histogram, duration, base::Seconds(1),
      kKioskSessionDurationHistogramLimit, kKioskHistogramBucketCount);
  ClearStartTime();
}

void AppSessionMetricsService::RecordPreviousKioskSessionCrashIfAny() {
  const base::Value::Dict& metrics_dict =
      prefs_->GetValueDict(prefs::kKioskMetrics);

  const auto* previous_start_time_value =
      metrics_dict.Find(kKioskSessionStartTime);
  if (!previous_start_time_value)
    return;
  auto previous_start_time = base::ValueToTime(previous_start_time_value);
  if (!previous_start_time.has_value())
    return;
  // Setup |start_time_| to the previous not correctly completed session's
  // start time. |start_time_| will be cleared once the crash session metrics
  // are recorded.
  start_time_ = previous_start_time.value();
  RecordKioskSessionCrashed();
}

size_t AppSessionMetricsService::RetrieveLastDaySessionCount(
    base::Time session_start_time) {
  const base::Value::Dict& metrics_dict =
      prefs_->GetValueDict(prefs::kKioskMetrics);
  const base::Value::List* previous_times = nullptr;

  const auto* times_value = metrics_dict.Find(kKioskSessionLastDayList);
  if (times_value) {
    previous_times = times_value->GetIfList();
    DCHECK(previous_times);
  }

  base::Value::List times;
  if (previous_times) {
    for (const auto& time : *previous_times) {
      if (base::ValueToTime(time).has_value() &&
          session_start_time - base::ValueToTime(time).value() <=
              base::Days(1)) {
        times.Append(time.Clone());
      }
    }
  }
  times.Append(base::TimeToValue(session_start_time));
  size_t result = times.size();

  start_time_ = session_start_time;

  base::Value::Dict result_value;
  result_value.Set(kKioskSessionLastDayList, std::move(times));
  result_value.Set(kKioskSessionStartTime, base::TimeToValue(start_time_));
  prefs_->SetDict(prefs::kKioskMetrics, std::move(result_value));
  return result;
}

void AppSessionMetricsService::ClearStartTime() {
  start_time_ = base::Time();
  const base::Value::Dict& metrics_dict =
      prefs_->GetValueDict(prefs::kKioskMetrics);

  base::Value::Dict new_metrics_dict = metrics_dict.Clone();
  DCHECK(new_metrics_dict.Remove(kKioskSessionStartTime));

  prefs_->SetDict(prefs::kKioskMetrics, std::move(new_metrics_dict));
  prefs_->CommitPendingWrite(base::DoNothing(), base::DoNothing());
}

}  // namespace chromeos
