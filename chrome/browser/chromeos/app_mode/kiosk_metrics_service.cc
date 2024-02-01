// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_metrics_service.h"

#include <string>
#include <vector>

#include "base/check.h"
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
#include "build/chromeos_buildflags.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

// Returns true if there is a new crash in `crash_dirs` after
// `previous_start_time`.
//
// crash_dirs          - the list of known directories with crash related files.
// previous_start_time - the start time of the previous kiosk session that is
//                       suspected to end with a crash.
bool IsPreviousKioskSessionCrashed(const std::vector<std::string>& crash_dirs,
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

void ClearMetricFromPrefs(const std::string& metric_name, PrefService* prefs) {
  ScopedDictPrefUpdate(prefs, prefs::kKioskMetrics)->Remove(metric_name);
  prefs->CommitPendingWrite(base::DoNothing(), base::DoNothing());
}

bool IsFirstSessionAfterReboot() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return user_manager::UserManager::Get()->IsFirstExecAfterBoot();
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

KioskSessionRestartReason RestartReasonWithRebootInfo(
    const KioskSessionRestartReason& initial_reason) {
  switch (initial_reason) {
    case KioskSessionRestartReason::kStopped:
      return IsFirstSessionAfterReboot()
                 ? KioskSessionRestartReason::kStoppedWithReboot
                 : KioskSessionRestartReason::kStopped;
    case KioskSessionRestartReason::kCrashed:
      return IsFirstSessionAfterReboot()
                 ? KioskSessionRestartReason::kCrashedWithReboot
                 : KioskSessionRestartReason::kCrashed;
    case KioskSessionRestartReason::kLocalStateWasNotSaved:
      return IsFirstSessionAfterReboot()
                 ? KioskSessionRestartReason::kLocalStateWasNotSavedWithReboot
                 : KioskSessionRestartReason::kLocalStateWasNotSaved;
    case KioskSessionRestartReason::kPluginCrashed:
      return IsFirstSessionAfterReboot()
                 ? KioskSessionRestartReason::kPluginCrashedWithReboot
                 : KioskSessionRestartReason::kPluginCrashed;
    case KioskSessionRestartReason::kPluginHung:
      return IsFirstSessionAfterReboot()
                 ? KioskSessionRestartReason::kPluginHungWithReboot
                 : KioskSessionRestartReason::kPluginHung;
    case KioskSessionRestartReason::kStoppedWithReboot:
    case KioskSessionRestartReason::kCrashedWithReboot:
    case KioskSessionRestartReason::kPluginCrashedWithReboot:
    case KioskSessionRestartReason::kPluginHungWithReboot:
    case KioskSessionRestartReason::kRebootPolicy:
    case KioskSessionRestartReason::kRemoteActionReboot:
    case KioskSessionRestartReason::kRestartApi:
    case KioskSessionRestartReason::kLocalStateWasNotSavedWithReboot:
      return initial_reason;
  }
}

KioskSessionRestartReason ConvertSessionEndReasonToSessionRestartReason(
    const KioskSessionEndReason& session_end_reason) {
  switch (session_end_reason) {
    case KioskSessionEndReason::kStopped:
      return RestartReasonWithRebootInfo(KioskSessionRestartReason::kStopped);
    case KioskSessionEndReason::kRebootPolicy:
      return KioskSessionRestartReason::kRebootPolicy;
    case KioskSessionEndReason::kRemoteActionReboot:
      return KioskSessionRestartReason::kRemoteActionReboot;
    case KioskSessionEndReason::kRestartApi:
      return KioskSessionRestartReason::kRestartApi;
    case KioskSessionEndReason::kPluginCrashed:
      return RestartReasonWithRebootInfo(
          KioskSessionRestartReason::kPluginCrashed);
    case KioskSessionEndReason::kPluginHung:
      return RestartReasonWithRebootInfo(
          KioskSessionRestartReason::kPluginHung);
  }
}

// If the session termination reason was not saved, returns an empty optional.
std::optional<KioskSessionEndReason> GetSessionEndReason(
    const PrefService* prefs) {
  const base::Value::Dict& metrics_dict = prefs->GetDict(prefs::kKioskMetrics);
  const auto* kiosk_session_stop_reason_value =
      metrics_dict.Find(kKioskSessionEndReason);
  if (!kiosk_session_stop_reason_value) {
    return std::nullopt;
  }
  auto kiosk_session_stop_reason = kiosk_session_stop_reason_value->GetIfInt();
  if (!kiosk_session_stop_reason.has_value()) {
    return std::nullopt;
  }

  return static_cast<KioskSessionEndReason>(kiosk_session_stop_reason.value());
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
const char kKioskSessionRestartReasonHistogram[] =
    "Kiosk.SessionRestart.Reason";
const char kKioskSessionLastDayList[] = "last-day-sessions";
const char kKioskSessionStartTime[] = "session-start-time";
const char kKioskSessionEndReason[] = "session-end-reason";

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

void KioskMetricsService::RecordKioskSessionStopped() {
  if (!IsKioskSessionRunning()) {
    return;
  }
  SaveSessionEndReason(KioskSessionEndReason::kStopped);
  RecordKioskSessionState(KioskSessionState::kStopped);
  RecordKioskSessionDuration(kKioskSessionDurationNormalHistogram,
                             kKioskSessionDurationInDaysNormalHistogram);
}

void KioskMetricsService::RecordPreviousKioskSessionCrashed(
    const base::Time& start_time) const {
  RecordKioskSessionState(KioskSessionState::kCrashed);
  RecordKioskSessionDuration(kKioskSessionDurationCrashedHistogram,
                             kKioskSessionDurationInDaysCrashedHistogram,
                             start_time);
}

void KioskMetricsService::RecordKioskSessionRestartReason(
    const KioskSessionRestartReason& reason) const {
  base::UmaHistogramEnumeration(kKioskSessionRestartReasonHistogram, reason);
}

void KioskMetricsService::RecordKioskSessionPluginCrashed() {
  SaveSessionEndReason(KioskSessionEndReason::kPluginCrashed);
  RecordKioskSessionState(KioskSessionState::kPluginCrashed);
  RecordKioskSessionDuration(kKioskSessionDurationCrashedHistogram,
                             kKioskSessionDurationInDaysCrashedHistogram);
}

void KioskMetricsService::RecordKioskSessionPluginHung() {
  SaveSessionEndReason(KioskSessionEndReason::kPluginHung);
  RecordKioskSessionState(KioskSessionState::kPluginHung);
  RecordKioskSessionDuration(kKioskSessionDurationCrashedHistogram,
                             kKioskSessionDurationInDaysCrashedHistogram);
}

void KioskMetricsService::RestartRequested(
    power_manager::RequestRestartReason reason) {
  switch (reason) {
    case power_manager::REQUEST_RESTART_FOR_USER:
    case power_manager::REQUEST_RESTART_FOR_UPDATE:
    case power_manager::REQUEST_RESTART_OTHER:
    case power_manager::REQUEST_RESTART_HEARTD:
      return;
    case power_manager::REQUEST_RESTART_SCHEDULED_REBOOT_POLICY:
      SaveSessionEndReason(KioskSessionEndReason::kRebootPolicy);
      return;
    case power_manager::REQUEST_RESTART_REMOTE_ACTION_REBOOT:
      SaveSessionEndReason(KioskSessionEndReason::kRemoteActionReboot);
      return;
    case power_manager::REQUEST_RESTART_API:
      SaveSessionEndReason(KioskSessionEndReason::kRestartApi);
      return;
  }
}

KioskMetricsService::KioskMetricsService(
    PrefService* prefs,
    const std::vector<std::string>& crash_dirs)
    : prefs_(prefs), crash_dirs_(crash_dirs) {
  auto* power_manager_client = chromeos::PowerManagerClient::Get();
  DCHECK(power_manager_client);
  power_manager_client_observation_.Observe(power_manager_client);
}

bool KioskMetricsService::IsKioskSessionRunning() const {
  return !start_time_.is_null();
}

void KioskMetricsService::RecordKioskSessionStarted(
    KioskSessionState started_state) {
  RecordPreviousKioskSessionEndState();
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

void KioskMetricsService::RecordPreviousKioskSessionEndState() {
  std::optional<KioskSessionEndReason> previous_session_end_reason =
      GetSessionEndReason(prefs_);
  // Avoid reading the old saved reason in the future.
  ClearMetricFromPrefs(kKioskSessionEndReason, prefs_);
  if (previous_session_end_reason.has_value()) {
    auto restart_reason = ConvertSessionEndReasonToSessionRestartReason(
        previous_session_end_reason.value());
    RecordKioskSessionRestartReason(restart_reason);
  }

  // Check for a previous session crash, as a crash may occur after the session
  // end reason was saved.
  const base::Value::Dict& metrics_dict = prefs_->GetDict(prefs::kKioskMetrics);
  auto previous_start_time =
      base::ValueToTime(metrics_dict.Find(kKioskSessionStartTime));
  if (!previous_start_time.has_value()) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&IsPreviousKioskSessionCrashed, crash_dirs_,
                     previous_start_time.value()),
      base::BindOnce(&KioskMetricsService::OnPreviousKioskSessionResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     previous_start_time.value(),
                     previous_session_end_reason.has_value()));
}

void KioskMetricsService::OnPreviousKioskSessionResult(
    const base::Time& start_time,
    bool has_recorded_session_restart_reason,
    bool crashed) const {
  if (crashed) {
    RecordPreviousKioskSessionCrashed(start_time);
    if (!has_recorded_session_restart_reason) {
      RecordKioskSessionRestartReason(
          RestartReasonWithRebootInfo(KioskSessionRestartReason::kCrashed));
    } else {
      SYSLOG(INFO)
          << "Kiosk session crash happend after recording end session reason";
    }
  } else if (!has_recorded_session_restart_reason) {
    // Previous session successfully stopped, but due to a race condition
    // local_state was not correctly updated.
    RecordKioskSessionRestartReason(RestartReasonWithRebootInfo(
        KioskSessionRestartReason::kLocalStateWasNotSaved));
  }
}

void KioskMetricsService::SaveSessionEndReason(
    const KioskSessionEndReason& reason) {
  if (prefs_->GetDict(prefs::kKioskMetrics).contains(kKioskSessionEndReason)) {
    // Do not override saved reason.
    // This function is called inside `RecordKioskSessionStopped` during the
    // destructor, but before that the actual restart reason could be saved from
    // different place.
    return;
  }

  ScopedDictPrefUpdate(prefs_, prefs::kKioskMetrics)
      ->Set(kKioskSessionEndReason, static_cast<int>(reason));
  prefs_->CommitPendingWrite(base::DoNothing(), base::DoNothing());
}

size_t KioskMetricsService::RetrieveLastDaySessionCount(
    base::Time session_start_time) {
  const base::Value::Dict& metrics_dict = prefs_->GetDict(prefs::kKioskMetrics);
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

  ScopedDictPrefUpdate(prefs_, prefs::kKioskMetrics)
      ->Set(kKioskSessionLastDayList, std::move(times));
  ScopedDictPrefUpdate(prefs_, prefs::kKioskMetrics)
      ->Set(kKioskSessionStartTime, base::TimeToValue(start_time_));
  return result;
}

void KioskMetricsService::ClearStartTime() {
  start_time_ = base::Time();
  ClearMetricFromPrefs(kKioskSessionStartTime, prefs_);
}

}  // namespace chromeos
