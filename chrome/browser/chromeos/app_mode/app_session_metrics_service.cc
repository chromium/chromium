// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_session_metrics_service.h"

#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/values_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/process/process_iterator.h"
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

// Returns true if there is a new crash in |crash_dirs| after
// |previous_start_time|.
//
// crash_dirs          - the list of known directories with crash related files.
// previous_start_time - the start time of the previous kiosk session that is
//                       suspected to end with a crash.
bool IsPreviousKioskSessionCrashed(const std::vector<std::string>& crash_dirs,
                                   const base::Time& previous_start_time) {
  for (const auto& crash_file_path : crash_dirs) {
    if (!base::PathExists(base::FilePath(crash_file_path)))
      continue;
    base::FileEnumerator enumerator(
        base::FilePath(crash_file_path), /* recursive= */ true,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    while (!enumerator.Next().empty()) {
      if (enumerator.GetInfo().GetLastModifiedTime() > previous_start_time) {
        // A new crash after |previous_start_time|.
        return true;
      }
    }
  }
  // No new crashes in |crash_dirs|.
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
const char kKioskRamUsagePercentageHistogram[] = "Kiosk.RamUsagePercentage";
const char kKioskSwapUsagePercentageHistogram[] = "Kiosk.SwapUsagePercentage";
const char kKioskDiskUsagePercentageHistogram[] = "Kiosk.DiskUsagePercentage";
const char kKioskChromeProcessCountHistogram[] = "Kiosk.ChromeProcessCount";
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
    DCHECK(base::PathService::Get(base::DIR_HOME, &path));
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
    : AppSessionMetricsService(prefs,
                               std::vector<std::string>(std::begin(kCrashDirs),
                                                        std::end(kCrashDirs))) {
}

AppSessionMetricsService::~AppSessionMetricsService() = default;

// static
std::unique_ptr<AppSessionMetricsService>
AppSessionMetricsService::CreateForTesting(
    PrefService* prefs,
    const std::vector<std::string>& crash_dirs) {
  return base::WrapUnique(new AppSessionMetricsService(prefs, crash_dirs));
}

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

void AppSessionMetricsService::RecordPreviousKioskSessionCrashed(
    const base::Time& start_time) const {
  RecordKioskSessionState(KioskSessionState::kCrashed);
  RecordKioskSessionDuration(kKioskSessionDurationCrashedHistogram,
                             kKioskSessionDurationInDaysCrashedHistogram,
                             start_time);
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

AppSessionMetricsService::AppSessionMetricsService(
    PrefService* prefs,
    const std::vector<std::string>& crash_dirs)
    : prefs_(prefs),
      disk_space_calculator_(std::make_unique<DiskSpaceCalculator>()),
      crash_dirs_(crash_dirs) {}

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
  RecordChromeProcessCount();
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

void AppSessionMetricsService::RecordChromeProcessCount() const {
  base::FilePath chrome_path;
  DCHECK(base::PathService::Get(base::FILE_EXE, &chrome_path));
  base::FilePath::StringType exe_name = chrome_path.BaseName().value();
  int process_count = base::GetProcessCount(exe_name, nullptr);
  base::UmaHistogramCounts100(kKioskChromeProcessCountHistogram, process_count);
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
  RecordKioskSessionDuration(kiosk_session_duration_histogram,
                             kiosk_session_duration_in_days_histogram,
                             start_time_);
  ClearStartTime();
}

void AppSessionMetricsService::RecordKioskSessionDuration(
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

void AppSessionMetricsService::RecordPreviousKioskSessionCrashIfAny() {
  const base::Value::Dict& metrics_dict = prefs_->GetDict(prefs::kKioskMetrics);
  const auto* previous_start_time_value =
      metrics_dict.Find(kKioskSessionStartTime);
  if (!previous_start_time_value)
    return;
  auto previous_start_time = base::ValueToTime(previous_start_time_value);
  if (!previous_start_time.has_value())
    return;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&IsPreviousKioskSessionCrashed, crash_dirs_,
                     previous_start_time.value()),
      base::BindOnce(&AppSessionMetricsService::OnPreviousKioskSessionResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     previous_start_time.value()));
}

void AppSessionMetricsService::OnPreviousKioskSessionResult(
    const base::Time& start_time,
    bool crashed) const {
  if (crashed) {
    RecordPreviousKioskSessionCrashed(start_time);
    return;
  }
  // Previous session is successfully stopped, but due to a race condition not
  // cleared local_state correctly.
  // Respective UMA metrics were emitted during the previous session.
}

size_t AppSessionMetricsService::RetrieveLastDaySessionCount(
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

  base::Value::Dict result_value;
  result_value.Set(kKioskSessionLastDayList, std::move(times));
  result_value.Set(kKioskSessionStartTime, base::TimeToValue(start_time_));
  prefs_->SetDict(prefs::kKioskMetrics, std::move(result_value));
  return result;
}

void AppSessionMetricsService::ClearStartTime() {
  start_time_ = base::Time();
  const base::Value::Dict& metrics_dict = prefs_->GetDict(prefs::kKioskMetrics);

  base::Value::Dict new_metrics_dict = metrics_dict.Clone();
  DCHECK(new_metrics_dict.Remove(kKioskSessionStartTime));

  prefs_->SetDict(prefs::kKioskMetrics, std::move(new_metrics_dict));
  prefs_->CommitPendingWrite(base::DoNothing(), base::DoNothing());
}

}  // namespace chromeos
