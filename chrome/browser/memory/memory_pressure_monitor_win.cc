// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_pressure_monitor_win.h"

#include <windows.h>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace memory {

namespace {

using SamplingFrequency = performance_monitor::SystemMonitor::SamplingFrequency;
using MetricsRefreshFrequencies = performance_monitor::SystemMonitor::
    SystemObserver::MetricRefreshFrequencies;

const DWORDLONG kMBBytes = 1024 * 1024;

// Disk idle time is usually almost null (according to the
// 'PerformanceMonitor.SystemMonitor.DiskIdleTime' metric) when the system is
// under memory pressure, and it's usually at 100% the rest of the time. Using a
// threshold of 30% should be a good indicator that there's a lot of I/O
// activity that might be caused by memory pressure if the free physical memory
// is low.
constexpr base::FeatureParam<double> kDiskIdleTimeLowThreshold{
    &features::kNewMemoryPressureMonitor, "DiskIdleTimeThreshold", 0.3};

// A disk idle time observation window of 6 seconds combined with the threshold
// specified above should be sufficient to determine that there's a high and
// sustained I/O activity.
static constexpr base::FeatureParam<int> kDiskIdleTimeWindowLengthSeconds{
    &features::kNewMemoryPressureMonitor, "DiskIdleTimeWindowLengthSeconds", 6};

// A system is considered 'high memory' if it has more than 1.5GB of system
// memory available for use by the memory manager (not reserved for hardware
// and drivers). This is a fuzzy version of the ~2GB discussed below.
const int kLargeMemoryThresholdMb = 1536;

// The limits below have been lifted from similar values in the
// base::MemoryPressureMonitor code. They have been slightly increased to
// account for the fact that the memory pressure signal aren't based solely on
// these values.

// These are the default thresholds used for systems with < ~2GB of physical
// memory. Such systems have been observed to always maintain ~100MB of
// available memory, paging until that is the case. To try to avoid paging a
// threshold slightly above this is chosen. The early threshold is slightly less
// grounded in reality and chosen as 2x critical.
static constexpr base::FeatureParam<int> kSmallMemoryDefaultEarlyThresholdMb{
    &features::kNewMemoryPressureMonitor, "SmallMemoryDefaultEarlyThresholdMb",
    600};
static constexpr base::FeatureParam<int> kSmallMemoryDefaultCriticalThresholdMb{
    &features::kNewMemoryPressureMonitor,
    "SmallMemoryDefaultCriticalThresholdMb", 300};

// These are the default thresholds used for systems with >= ~2GB of physical
// memory. Such systems have been observed to always maintain ~300MB of
// available memory, paging until that is the case.
static constexpr base::FeatureParam<int> kLargeMemoryDefaultEarlyThresholdMb{
    &features::kNewMemoryPressureMonitor, "LargeMemoryDefaultEarlyThresholdMb",
    1000};
static constexpr base::FeatureParam<int> kLargeMemoryDefaultCriticalThresholdMb{
    &features::kNewMemoryPressureMonitor,
    "LargeMemoryDefaultCriticalThresholdMb", 500};

// A window length of 10 seconds should be sufficient to determine that the
// system is under pressure. A shorter window will lead to too many false
// positives (e.g. a brief memory spike will be treated as a memory pressure
// event) and a longer one will delay the response to memory pressure.
static constexpr base::FeatureParam<int> kFreeMemoryWindowLengthSeconds{
    &features::kNewMemoryPressureMonitor, "FreeMemoryWindowLengthSeconds", 10};

// If 40% of the samples in the free physical memory observation window have a
// value lower than one of the threshold then the window will consider that the
// memory is under this limit. This makes this signal more stable if the memory
// varies a lot (which can happen if the system is actively trying to free some
// memory).
constexpr base::FeatureParam<double> kFreeMemorySampleRatioToBePositive{
    &features::kNewMemoryPressureMonitor, "FreeMemorySampleRatioToBePositive",
    0.4};

FreeMemoryObservationWindow::Config GetFreeMemoryWindowConfig() {
  // Default to a 'high' memory situation, which uses more conservative
  // thresholds.
  bool high_memory = true;
  MEMORYSTATUSEX mem_status = {};
  mem_status.dwLength = sizeof(mem_status);
  if (::GlobalMemoryStatusEx(&mem_status)) {
    static const DWORDLONG kLargeMemoryThresholdBytes =
        static_cast<DWORDLONG>(kLargeMemoryThresholdMb) * kMBBytes;
    high_memory = mem_status.ullTotalPhys >= kLargeMemoryThresholdBytes;
  }

  int low_memory_early_limit_mb = kSmallMemoryDefaultEarlyThresholdMb.Get();
  int low_memory_critical_limit_mb =
      kSmallMemoryDefaultCriticalThresholdMb.Get();

  if (high_memory) {
    low_memory_early_limit_mb = kLargeMemoryDefaultEarlyThresholdMb.Get();
    low_memory_critical_limit_mb = kLargeMemoryDefaultCriticalThresholdMb.Get();
  }

  return {
      .sample_ratio_to_be_positive = kFreeMemorySampleRatioToBePositive.Get(),
      .low_memory_early_limit_mb = low_memory_early_limit_mb,
      .low_memory_critical_limit_mb = low_memory_critical_limit_mb,
  };
}

}  // namespace

MemoryPressureMonitorWin::MemoryPressureMonitorWin()
    : free_memory_obs_window_(
          base::TimeDelta::FromSeconds(kFreeMemoryWindowLengthSeconds.Get()),
          GetFreeMemoryWindowConfig()),
      disk_idle_time_obs_window_(
          base::TimeDelta::FromSeconds(kDiskIdleTimeWindowLengthSeconds.Get()),
          kDiskIdleTimeLowThreshold.Get()) {
  DCHECK(performance_monitor::SystemMonitor::Get());
  // The amount of free memory is always tracked.
  refresh_frequencies_ =
      MetricsRefreshFrequencies::Builder()
          .SetFreePhysMemoryMbFrequency(SamplingFrequency::kDefaultFrequency)
          .Build();
  performance_monitor::SystemMonitor::Get()->AddOrUpdateObserver(
      this, refresh_frequencies_);
}

MemoryPressureMonitorWin::~MemoryPressureMonitorWin() = default;

void MemoryPressureMonitorWin::OnFreePhysicalMemoryMbSample(
    int free_phys_memory_mb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  free_memory_obs_window_.OnSample(free_phys_memory_mb);
  OnObservationWindowUpdate();
}

void MemoryPressureMonitorWin::OnDiskIdleTimePercent(
    float disk_idle_time_percent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  disk_idle_time_obs_window_.OnSample(disk_idle_time_percent);
  OnObservationWindowUpdate();
}

void MemoryPressureMonitorWin::OnObservationWindowUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::MemoryPressureListener::MemoryPressureLevel new_level =
      MemoryPressureMonitorWin::CheckObservationWindowsAndComputeLevel();

  if (new_level != memory_pressure_level()) {
    // TODO(sebmarchand): Emit some metrics here that compare this signal
    // against the legacy one and against the SwapThrashingMonitor if it's
    // enabled.
    OnMemoryPressureLevelChange(new_level);
  }
}

base::MemoryPressureListener::MemoryPressureLevel
MemoryPressureMonitorWin::CheckObservationWindowsAndComputeLevel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (free_memory_obs_window_.MemoryIsUnderEarlyLimit()) {
    // Enable the tracking of the disk idle time if needed.
    if (refresh_frequencies_.disk_idle_time_percent_frequency ==
        SamplingFrequency::kNoSampling) {
      refresh_frequencies_.disk_idle_time_percent_frequency =
          SamplingFrequency::kDefaultFrequency;
      performance_monitor::SystemMonitor::Get()->AddOrUpdateObserver(
          this, refresh_frequencies_);
      // Don't return here, the disk idle time observation window might already
      // contain enough recent samples to make a decision.
      //
      // TODO(sebmarchand): Maybe set the memory pressure level as moderate?
      // It's not clear of what moderate will mean with the new signal yet (it
      // might simply go away) but it could maybe be used as an internal level
      // anyway.
    }
  } else {
    // Disable the tracking of the disk idle time if it's enabled.
    if (refresh_frequencies_.disk_idle_time_percent_frequency !=
        SamplingFrequency::kNoSampling) {
      refresh_frequencies_.disk_idle_time_percent_frequency =
          SamplingFrequency::kNoSampling;
      performance_monitor::SystemMonitor::Get()->AddOrUpdateObserver(
          this, refresh_frequencies_);
      // No need to continue after this.
      return base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_NONE;
    }
  }

  // Check all the conditions one by one, if they're all true then the system is
  // under pressure.

  if (!free_memory_obs_window_.MemoryIsUnderCriticalLimit()) {
    return base::MemoryPressureListener::MemoryPressureLevel::
        MEMORY_PRESSURE_LEVEL_NONE;
  }

  if (!disk_idle_time_obs_window_.DiskIdleTimeIsLow()) {
    return base::MemoryPressureListener::MemoryPressureLevel::
        MEMORY_PRESSURE_LEVEL_NONE;
  }

  // The system is under memory pressure.
  return base::MemoryPressureListener::MemoryPressureLevel::
      MEMORY_PRESSURE_LEVEL_CRITICAL;
}

}  // namespace memory
