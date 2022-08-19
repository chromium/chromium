// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_session_metrics_service.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
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

void ReportUsedPercentage(const char* histogram_name,
                          int64_t available,
                          int64_t total) {
  int percents;
  if (total <= 0 || available < 0 || total < available) {
    percents = 100;
  } else {
    percents = (total - available) * 100 / total;
  }
  base::UmaHistogramPercentage(histogram_name, percents);
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
const char kKioskRamUsagePercentageHistogram[] = "Kiosk.RamUsagePercentage";
const char kKioskSwapUsagePercentageHistogram[] = "Kiosk.SwapUsagePercentage";
const char kKioskDiskUsagePercentageHistogram[] = "Kiosk.DiskUsagePercentage";
const char kKioskSessionLastDayList[] = "last-day-sessions";
const char kKioskSessionStartTime[] = "session-start-time";

const int kKioskHistogramBucketCount = 100;
const base::TimeDelta kKioskSessionDurationHistogramLimit = base::Days(1);
const base::TimeDelta kPeriodicMetricsInterval = base::Hours(1);

// This class is calculating amount of available and total disk space and
// reports the percentage of available disk space to the histogram. Since the
// calculation contains a blocking call, this is done asynchronously.
class DiskSpaceCalculator {
 public:
  struct DiskSpaceInfo {
    int64_t free_bytes;
    int64_t total_bytes;
  };
  void StartCalculation() {
    base::FilePath path;
    if (!base::PathService::Get(base::DIR_HOME, &path)) {
      return;
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&DiskSpaceCalculator::GetDiskSpaceBlocking, path),
        base::BindOnce(&DiskSpaceCalculator::OnReceived,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  static DiskSpaceInfo GetDiskSpaceBlocking(const base::FilePath& mount_path) {
    int64_t free_bytes = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
    int64_t total_bytes = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
    return DiskSpaceInfo{free_bytes, total_bytes};
  }

 private:
  void OnReceived(const DiskSpaceInfo& disk_info) {
    ReportUsedPercentage(kKioskDiskUsagePercentageHistogram,
                         disk_info.free_bytes, disk_info.total_bytes);
  }

  base::WeakPtrFactory<DiskSpaceCalculator> weak_ptr_factory_{this};
};

AppSessionMetricsService::AppSessionMetricsService(PrefService* prefs)
    : prefs_(prefs),
      disk_space_calculator_(std::make_unique<DiskSpaceCalculator>()) {}

AppSessionMetricsService::~AppSessionMetricsService() = default;

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
  StartMetricsTimer();
}

void AppSessionMetricsService::StartMetricsTimer() {
  metrics_timer_.Start(FROM_HERE, kPeriodicMetricsInterval, this,
                       &AppSessionMetricsService::RecordPeriodicMetrics);
}

void AppSessionMetricsService::RecordPeriodicMetrics() {
  RecordRamUsage();
  RecordSwapUsage();
  RecordDiskSpaceUsage();
}

void AppSessionMetricsService::RecordRamUsage() const {
  int64_t available_ram = base::SysInfo::AmountOfAvailablePhysicalMemory();
  int64_t total_ram = base::SysInfo::AmountOfPhysicalMemory();
  ReportUsedPercentage(kKioskRamUsagePercentageHistogram, available_ram,
                       total_ram);
}

void AppSessionMetricsService::RecordSwapUsage() const {
  base::SystemMemoryInfoKB memory;
  if (!base::GetSystemMemoryInfo(&memory)) {
    return;
  }
  int64_t swap_free = memory.swap_free;
  int64_t swap_total = memory.swap_total;
  ReportUsedPercentage(kKioskSwapUsagePercentageHistogram, swap_free,
                       swap_total);
}

void AppSessionMetricsService::RecordDiskSpaceUsage() const {
  DCHECK(disk_space_calculator_);
  disk_space_calculator_->StartCalculation();
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
