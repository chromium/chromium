// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_metrics_service.h"

#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

namespace {

// Info on crash report locations:
// docs/website/site/chromium-os/packages/crash-reporting/faq/index.md
const constexpr char* kCrashDirs[] = {
    "/home/chronos/crash",      // crashes outside user session. may happen on
                                // chromium shutdown
    "/home/chronos/user/crash"  // crashes inside user/kiosk session
};

bool IsRestoredSession() {
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
}

// Returns true if there is a new crash in `crash_dirs` after
// `previous_start_time`.
//
// crash_dirs          - the list of known directories with crash related files.
// previous_start_time - the start time of the previous kiosk session that is
//                       suspected to end with a crash.
bool HasNewCrashFiles(const std::vector<std::string>& crash_dirs,
                      const base::Time& previous_start_time) {
  for (const auto& crash_file_path : crash_dirs) {
    if (!base::PathExists(base::FilePath(crash_file_path))) {
      continue;
    }
    base::FileEnumerator enumerator(
        base::FilePath(crash_file_path), /*recursive=*/true,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    while (!enumerator.Next().empty()) {
      if (enumerator.GetInfo().GetLastModifiedTime() > previous_start_time) {
        // A new crash after `previous_start_time`.
        return true;
      }
    }
  }
  // No new crashes in `crash_dirs`.
  return false;
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

KioskMetricsService::KioskMetricsService(PrefService* prefs)
    : KioskMetricsService(prefs,
                          std::vector<std::string>(std::begin(kCrashDirs),
                                                   std::end(kCrashDirs))) {}

KioskMetricsService::~KioskMetricsService() = default;

// static
std::unique_ptr<KioskMetricsService> KioskMetricsService::CreateForTesting(
    PrefService* prefs,
    const std::vector<std::string>& crash_dirs) {
  return base::WrapUnique(new KioskMetricsService(prefs, crash_dirs));
}

void KioskMetricsService::RecordKioskSessionStarted() {
  RecordKioskSessionStarted(KioskSessionState::kStarted);
}

void KioskMetricsService::RecordKioskSessionWebStarted() {
  RecordKioskSessionStarted(KioskSessionState::kWebStarted);
}

void KioskMetricsService::RecordKioskSessionIwaStarted() {
  RecordKioskSessionStarted(KioskSessionState::kIwaStarted);
}

void KioskMetricsService::RecordKioskSessionStopped() {
  if (!IsKioskSessionRunning()) {
    return;
  }
  RecordKioskSessionState(KioskSessionState::kStopped);
  RecordKioskSessionDuration(kKioskSessionDurationNormalHistogram,
                             kKioskSessionDurationInDaysNormalHistogram);
}

void KioskMetricsService::RecordKioskSessionPluginHung() {
  RecordKioskSessionState(KioskSessionState::kPluginHung);
  RecordKioskSessionDuration(kKioskSessionDurationCrashedHistogram,
                             kKioskSessionDurationInDaysCrashedHistogram);
}

KioskMetricsService::KioskMetricsService(
    PrefService* prefs,
    const std::vector<std::string>& crash_dirs)
    : prefs_(prefs), crash_dirs_(crash_dirs) {}

bool KioskMetricsService::IsKioskSessionRunning() const {
  return !start_time_.is_null();
}

void KioskMetricsService::RecordKioskSessionStarted(
    KioskSessionState started_state) {
  CheckIfPreviousSessionCrashed();
  if (IsRestoredSession()) {
    RecordKioskSessionState(KioskSessionState::kRestored);
  } else {
    RecordKioskSessionState(started_state);
  }
  RecordKioskSessionCountPerDay();
}

void KioskMetricsService::RecordKioskSessionState(
    KioskSessionState state) const {
  base::UmaHistogramEnumeration(kKioskSessionStateHistogram, state);
}

void KioskMetricsService::RecordKioskSessionCountPerDay() {
  base::UmaHistogramCounts100(kKioskSessionCountPerDayHistogram,
                              RetrieveLastDaySessionCount(base::Time::Now()));
}

void KioskMetricsService::RecordKioskSessionDuration(
    const std::string& kiosk_session_duration_histogram,
    const std::string& kiosk_session_duration_in_days_histogram) {
  if (!IsKioskSessionRunning()) {
    return;
  }
  RecordKioskSessionDuration(kiosk_session_duration_histogram,
                             kiosk_session_duration_in_days_histogram,
                             start_time_);
  ClearStartTime();
}

void KioskMetricsService::RecordKioskSessionDuration(
    const std::string& kiosk_session_duration_histogram,
    const std::string& kiosk_session_duration_in_days_histogram,
    const base::Time& start_time) const {
  base::TimeDelta duration = base::Time::Now() - start_time;
  if (duration >= kKioskSessionDurationHistogramLimit) {
    base::UmaHistogramCounts100(kiosk_session_duration_in_days_histogram,
                                std::min(100, duration.InDays()));
    duration = kKioskSessionDurationHistogramLimit;
  }
  base::UmaHistogramCustomTimes(
      kiosk_session_duration_histogram, duration, base::Seconds(1),
      kKioskSessionDurationHistogramLimit, kKioskHistogramBucketCount);
}

void KioskMetricsService::CheckIfPreviousSessionCrashed() {
  const base::DictValue& metrics_dict = prefs_->GetDict(prefs::kKioskMetrics);
  auto previous_start_time =
      base::ValueToTime(metrics_dict.Find(kKioskSessionStartTime));
  if (!previous_start_time.has_value()) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&HasNewCrashFiles, crash_dirs_,
                     previous_start_time.value()),
      base::BindOnce(&KioskMetricsService::RecordPreviousKioskSessionCrashed,
                     weak_ptr_factory_.GetWeakPtr(),
                     previous_start_time.value()));
}

void KioskMetricsService::RecordPreviousKioskSessionCrashed(
    const base::Time& start_time,
    bool crashed) const {
  if (crashed) {
    RecordKioskSessionState(KioskSessionState::kCrashed);
    RecordKioskSessionDuration(kKioskSessionDurationCrashedHistogram,
                               kKioskSessionDurationInDaysCrashedHistogram,
                               start_time);
  }
}

size_t KioskMetricsService::RetrieveLastDaySessionCount(
    base::Time session_start_time) {
  const base::DictValue& metrics_dict = prefs_->GetDict(prefs::kKioskMetrics);
  const base::ListValue* previous_times = nullptr;

  const auto* times_value = metrics_dict.Find(kKioskSessionLastDayList);
  if (times_value) {
    previous_times = times_value->GetIfList();
    DCHECK(previous_times);
  }

  base::ListValue times;
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

  ScopedDictPrefUpdate(prefs_, prefs::kKioskMetrics)
      ->Set(kKioskSessionLastDayList, std::move(times));
  ScopedDictPrefUpdate(prefs_, prefs::kKioskMetrics)
      ->Set(kKioskSessionStartTime, base::TimeToValue(start_time_));
  return result;
}

void KioskMetricsService::ClearStartTime() {
  start_time_ = base::Time();

  // Clear metric from prefs.
  ScopedDictPrefUpdate(prefs_, prefs::kKioskMetrics)
      ->Remove(kKioskSessionStartTime);
  prefs_->CommitPendingWrite(base::DoNothing(), base::DoNothing());
}

}  // namespace chromeos
