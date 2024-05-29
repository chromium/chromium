// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/metrics_provider_desktop.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/cpu_frequency_utils.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform_node.h"

using performance_manager::user_tuning::prefs::kMemorySaverModeState;
using performance_manager::user_tuning::prefs::MemorySaverModeState;

namespace performance_manager {

namespace {

MetricsProviderDesktop* g_metrics_provider = nullptr;

uint64_t kBytesPerMb = 1024 * 1024;

#if BUILDFLAG(IS_MAC)
uint64_t kKilobytesPerMb = 1024;
#endif

base::TimeDelta kCpuThroughputSamplingInterval = base::Minutes(5);

}  // namespace

// Tracks the proportion of time a specific mode was enabled during this
// object's entire lifetime, and records it to a specified histogram on
// destruction.
class ScopedTimeInModeTracker {
 public:
  ScopedTimeInModeTracker(bool enabled, const std::string& histogram_name)
      : currently_enabled_(enabled),
        current_interval_start_(base::LiveTicks::Now()),
        start_(current_interval_start_),
        histogram_name_(histogram_name) {}

  ~ScopedTimeInModeTracker() {
    // Ensure `time_spent_enabled_` is updated if the mode was currently
    // enabled. This doesn't call `ModeChanged` directly to ensure the value of
    // `now` used for the total time computation is the same as was used to
    // close the interval.
    base::LiveTicks now = base::LiveTicks::Now();

    CHECK(current_interval_start_ <= now);
    CHECK(start_ <= now);

    if (currently_enabled_) {
      time_spent_enabled_ += now - current_interval_start_;
    }

    base::TimeDelta total_time = now - start_;

    // Time spent enabled should be lower or equal to the total time this was
    // active.
    CHECK_LE(time_spent_enabled_, total_time);
    // Check that the `time_spent_enabled_ * 100` operation can't overflow.
    CHECK_LE(time_spent_enabled_.InMicroseconds(),
             std::numeric_limits<int64_t>::max() / 100);

    // `total_time` being 0 would mean the object was constructed and destructed
    // without the clock advancing a single microsecond. This shouldn't happen
    // in production but can happen in tests that use mock time. Treat this as
    // an interval that has only been in the current state.
    unsigned int percent_enabled = currently_enabled_ ? 100 : 0;
    if (total_time.is_positive()) {
      // Do the computation in microseconds to avoid prior truncation since it's
      // `TimeDelta`'s internal representation.
      int64_t checked_percent =
          (base::CheckMul(time_spent_enabled_.InMicroseconds(), 100) /
           total_time.InMicroseconds())
              .ValueOrDie();

      CHECK(base::IsValueInRangeForNumericType<unsigned int>(checked_percent));

      percent_enabled = checked_percent;
    }

    CHECK_LE(percent_enabled, 100U);

    base::UmaHistogramPercentage(histogram_name_, percent_enabled);
  }

  void ModeChanged(bool enabled) {
    if (currently_enabled_ == enabled) {
      // It's possible for the pref to be notified as "changed" even if it's
      // "changing" to the same state it's already in when going to/from
      // "enabled with heuristic mode" to/from "enabled on timer mode".
      return;
    }

    base::LiveTicks now = base::LiveTicks::Now();

    CHECK(current_interval_start_ <= now);

    if (currently_enabled_) {
      time_spent_enabled_ += now - current_interval_start_;
    }

    currently_enabled_ = enabled;
    current_interval_start_ = now;
  }

 private:
  bool currently_enabled_;

  base::TimeDelta time_spent_enabled_;
  base::LiveTicks current_interval_start_;
  base::LiveTicks start_;

  std::string histogram_name_;
};

// static
MetricsProviderDesktop* MetricsProviderDesktop::GetInstance() {
  DCHECK(g_metrics_provider);
  return g_metrics_provider;
}

MetricsProviderDesktop::~MetricsProviderDesktop() {
  DCHECK_EQ(this, g_metrics_provider);
  g_metrics_provider = nullptr;
}

void MetricsProviderDesktop::Initialize() {
  DCHECK(!initialized_);

  pref_change_registrar_.Init(local_state_);
  pref_change_registrar_.Add(
      kMemorySaverModeState,
      base::BindRepeating(&MetricsProviderDesktop::OnMemorySaverPrefChanged,
                          base::Unretained(this)));
  performance_manager::user_tuning::BatterySaverModeManager::GetInstance()
      ->AddObserver(this);
  battery_saver_enabled_ =
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance()
          ->IsBatterySaverActive();

  initialized_ = true;
  current_mode_ = ComputeCurrentMode();

  ResetTrackers();

  PostDiskMetricsTask();
}

void MetricsProviderDesktop::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // It's valid for this to be called when `initialized_` is false if the finch
  // features controlling battery saver and memory saver are disabled.
  // TODO(crbug.com/40233418): CHECK(initialized_) when the features are enabled
  // and removed.
  base::UmaHistogramEnumeration("PerformanceManager.UserTuning.EfficiencyMode",
                                current_mode_);

  // Resetting the trackers will cause the existing ones to record their
  // histogram.
  ResetTrackers();

  // Set `current_mode_` to represent the state of the modes as they are now, so
  // that this mode is what is adequately reported at the next report, unless it
  // changes in the meantime.
  current_mode_ = ComputeCurrentMode();

  RecordDiskMetrics();

  // Request a disk measurement so it's ready for the next interval
  PostDiskMetricsTask();
}

MetricsProviderDesktop::MetricsProviderDesktop(PrefService* local_state)
    : local_state_(local_state),
      disk_metrics_getter_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
  DCHECK(!g_metrics_provider);
  g_metrics_provider = this;

  available_memory_metrics_timer_.Start(
      FROM_HERE, base::Minutes(2),
      base::BindRepeating(&MetricsProviderDesktop::RecordAvailableMemoryMetrics,
                          base::Unretained(this)));

  if constexpr (ShouldCollectCpuFrequencyMetrics()) {
    ScheduleCpuFrequencyTask();
  }
}

void MetricsProviderDesktop::OnBatterySaverActiveChanged(bool is_active) {
  battery_saver_enabled_ = is_active;
  battery_saver_mode_tracker_->ModeChanged(battery_saver_enabled_);
  OnTuningModesChanged();
}

void MetricsProviderDesktop::OnMemorySaverPrefChanged() {
  memory_saver_mode_tracker_->ModeChanged(IsMemorySaverEnabled());
  OnTuningModesChanged();
}

void MetricsProviderDesktop::OnTuningModesChanged() {
  EfficiencyMode new_mode = ComputeCurrentMode();

  // If the mode changes between UMA reports, mark it as Mixed for this
  // interval.
  if (current_mode_ != new_mode) {
    current_mode_ = EfficiencyMode::kMixed;
  }
}

MetricsProviderDesktop::EfficiencyMode
MetricsProviderDesktop::ComputeCurrentMode() const {
  // It's valid for this to be uninitialized if the battery saver/high
  // efficiency modes are unavailable. In that case, the browser is running in
  // normal mode, so return kNormal.
  // TODO(crbug.com/40233418): Change this to a DCHECK when the features are
  // enabled and removed.
  if (!initialized_) {
    return EfficiencyMode::kNormal;
  }

  // It's possible for this function to be called during shutdown, after
  // BatterySaverModeManager is destroyed. Do not access UPTM directly from
  // here.

  bool high_efficiency_enabled = IsMemorySaverEnabled();

  if (high_efficiency_enabled && battery_saver_enabled_) {
    return EfficiencyMode::kBoth;
  }

  if (high_efficiency_enabled) {
    return EfficiencyMode::kMemorySaver;
  }

  if (battery_saver_enabled_) {
    return EfficiencyMode::kBatterySaver;
  }

  return EfficiencyMode::kNormal;
}

bool MetricsProviderDesktop::IsMemorySaverEnabled() const {
  return local_state_->GetInteger(kMemorySaverModeState) !=
         static_cast<int>(MemorySaverModeState::kDisabled);
}

void MetricsProviderDesktop::RecordAvailableMemoryMetrics() {
  auto available_bytes = base::SysInfo::AmountOfAvailablePhysicalMemory();
  auto total_bytes = base::SysInfo::AmountOfPhysicalMemory();

  base::UmaHistogramMemoryLargeMB("Memory.Experimental.AvailableMemoryMB",
                                  available_bytes / kBytesPerMb);
  base::UmaHistogramPercentage("Memory.Experimental.AvailableMemoryPercent",
                               available_bytes * 100 / total_bytes);

#if BUILDFLAG(IS_MAC)
  base::SystemMemoryInfoKB info;
  if (base::GetSystemMemoryInfo(&info)) {
    base::UmaHistogramMemoryLargeMB(
        "Memory.Experimental.MacFileBackedMemoryMB2",
        info.file_backed / kKilobytesPerMb);
    // `info.file_backed` is in kb, so multiply it by 1024 to get the amount of
    // bytes
    base::UmaHistogramPercentage(
        "Memory.Experimental.MacAvailableMemoryPercentFreePageCache2",
        (available_bytes + (info.file_backed * 1024)) * 100 / total_bytes);
  }
#endif
}

void MetricsProviderDesktop::ResetTrackers() {
  battery_saver_mode_tracker_ = std::make_unique<ScopedTimeInModeTracker>(
      battery_saver_enabled_,
      "PerformanceManager.UserTuning.BatterySaverModeEnabledPercent");
  memory_saver_mode_tracker_ = std::make_unique<ScopedTimeInModeTracker>(
      IsMemorySaverEnabled(),
      "PerformanceManager.UserTuning.MemorySaverModeEnabledPercent");
}

// static
void MetricsProviderDesktop::RecordCpuFrequencyMetrics(
    base::TimeTicks posted_at_time) {
  CHECK(ShouldCollectCpuFrequencyMetrics());

  auto queued_time = base::TimeTicks::Now() - posted_at_time;

  static const double kHzInMhz = 1000 * 1000;

  std::optional<base::CpuThroughputEstimationResult> cpu_throughput =
      base::EstimateCpuThroughput();
  base::CpuFrequencyInfo cpu_frequency_info = base::GetCpuFrequencyInfo();

  if (!cpu_throughput) {
    return;
  }

  std::string_view core_type_suffix = "Performance";
  if (cpu_frequency_info.type == base::CpuFrequencyInfo::CoreType::kBalanced) {
    core_type_suffix = "Balanced";
  } else if (cpu_frequency_info.type ==
             base::CpuFrequencyInfo::CoreType::kEfficiency) {
    core_type_suffix = "Efficiency";
  }

  base::UmaHistogramCustomMicrosecondsTimes(
      base::StrCat(
          {"CPU.Experimental.CpuEstimationTaskQueuedTime.", core_type_suffix}),
      queued_time, base::Microseconds(1), base::Seconds(1), 50);

  base::UmaHistogramCustomMicrosecondsTimes(
      base::StrCat(
          {"CPU.Experimental.CpuEstimationTaskTotalTime.", core_type_suffix}),
      queued_time + cpu_throughput->wall_time, base::Microseconds(1),
      base::Seconds(1), 50);

  base::UmaHistogramCustomMicrosecondsTimes(
      base::StrCat(
          {"CPU.Experimental.CpuEstimationTaskThreadTime.", core_type_suffix}),
      cpu_throughput->thread_time, base::Microseconds(1), base::Seconds(1), 50);

  base::UmaHistogramCustomMicrosecondsTimes(
      base::StrCat(
          {"CPU.Experimental.CpuEstimationTaskWallTime.", core_type_suffix}),
      cpu_throughput->wall_time, base::Microseconds(1), base::Seconds(1), 50);

  base::UmaHistogramBoolean("CPU.Experimental.CpuEstimationTaskMigrated",
                            cpu_throughput->migrated);

  // These can be 0 in tests
  if (!cpu_throughput->thread_time.is_zero() &&
      !cpu_throughput->wall_time.is_zero()) {
    base::UmaHistogramPercentage(
        base::StrCat({"CPU.Experimental.CpuEstimationThreadTimePercent.",
                      core_type_suffix}),
        static_cast<int>(cpu_throughput->thread_time /
                         cpu_throughput->wall_time * 100.0));
  }

  if (cpu_throughput->migrated) {
    // Don't record frequency metrics if the code migrated from one CPU to
    // another in the middle of the estimation loop. This is because the nominal
    // frequency of the start and end cores might be different.
    return;
  }

  double estimated_mhz = cpu_throughput->estimated_frequency / kHzInMhz;

  // Max/Limit can (rarely) be 0 in the field, perhaps in virtualized or
  // sandboxed environments.
  if (cpu_frequency_info.max_mhz > 0UL) {
    base::UmaHistogramPercentage(
        base::StrCat({"CPU.Experimental.EstimatedFrequencyAsPercentOfMax.",
                      core_type_suffix}),
        static_cast<int>(estimated_mhz * 100.0 /
                         static_cast<double>(cpu_frequency_info.max_mhz)));
  }

  if (cpu_frequency_info.mhz_limit > 0UL) {
    base::UmaHistogramPercentage(
        base::StrCat({"CPU.Experimental.EstimatedFrequencyAsPercentOfLimit.",
                      core_type_suffix}),
        static_cast<int>(estimated_mhz * 100.0 /
                         static_cast<double>(cpu_frequency_info.mhz_limit)));
  }

  ScheduleCpuFrequencyTask();
}

// static
void MetricsProviderDesktop::ScheduleCpuFrequencyTask() {
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&MetricsProviderDesktop::PostCpuFrequencyEstimation),
      kCpuThroughputSamplingInterval);
}

// static
void MetricsProviderDesktop::PostCpuFrequencyEstimation() {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&MetricsProviderDesktop::RecordCpuFrequencyMetrics,
                     base::TimeTicks::Now()));
}

void MetricsProviderDesktop::RecordDiskMetrics() {
  if (!pending_disk_metrics_) {
    // The measurements aren't ready yet, don't report anything.
    return;
  }

  if (pending_disk_metrics_->free_bytes == -1 ||
      pending_disk_metrics_->total_bytes == -1) {
    return;
  }

  base::UmaHistogramCustomCounts(
      "PerformanceManager.DiskStats.UserDataDirFreeSpaceMb",
      pending_disk_metrics_->free_bytes /
          kBytesPerMb,  // space_info is bytes, convert to Mb
      0, 10240,  // It's fine to bucket everything >10Gb as "large enough"
      100);
  // Also report as a percentage of capacity
  base::UmaHistogramPercentage(
      "PerformanceManager.DiskStats.UserDataDirFreeSpacePercent",
      pending_disk_metrics_->free_bytes * 100 /
          pending_disk_metrics_->total_bytes);

  pending_disk_metrics_ = std::nullopt;
}

void MetricsProviderDesktop::PostDiskMetricsTask() {
  if (!g_browser_process || !g_browser_process->profile_manager()) {
    // It's possible to have a null browser process or a null profile manager in
    // unit tests.
    return;
  }

  // Records the free/available space on the disk that hosts the user data dir.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const base::FilePath& user_data_dir = profile_manager->user_data_dir();

  disk_metrics_getter_
      .AsyncCall(&MetricsProviderDesktop::DiskMetricsThreadPoolGetter::
                     ComputeDiskMetrics)
      .WithArgs(user_data_dir)
      .Then(base::BindOnce(&MetricsProviderDesktop::SavePendingDiskMetrics,
                           base::Unretained(this)));
}

MetricsProviderDesktop::DiskMetrics
MetricsProviderDesktop::DiskMetricsThreadPoolGetter::ComputeDiskMetrics(
    const base::FilePath& user_data_dir) {
  return {
      .free_bytes = base::SysInfo::AmountOfFreeDiskSpace(user_data_dir),
      .total_bytes = base::SysInfo::AmountOfTotalDiskSpace(user_data_dir),
  };
}

void MetricsProviderDesktop::SavePendingDiskMetrics(DiskMetrics metrics) {
  pending_disk_metrics_ = metrics;
}

}  // namespace performance_manager
