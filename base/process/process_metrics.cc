// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <utility>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "build/build_config.h"

namespace base {

namespace {

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_AIX)
int CalculateEventsPerSecond(uint64_t event_count,
                             uint64_t* last_event_count,
                             base::TimeTicks* last_calculated) {
  const base::TimeTicks time = base::TimeTicks::Now();

  int events_per_second = 0;
  if (*last_event_count != 0) {
    const uint64_t events_delta = event_count - *last_event_count;
    const base::TimeDelta time_delta = time - *last_calculated;
    DCHECK(!time_delta.is_zero());
    events_per_second = ClampRound(events_delta / time_delta.InSecondsF());
  }

  *last_calculated = time;
  *last_event_count = event_count;
  return events_per_second;
}
#endif

}  // namespace

SystemMemoryInfoKB::SystemMemoryInfoKB() = default;

SystemMemoryInfoKB::SystemMemoryInfoKB(const SystemMemoryInfoKB&) = default;

SystemMemoryInfoKB& SystemMemoryInfoKB::operator=(const SystemMemoryInfoKB&) =
    default;

SystemMetrics::SystemMetrics() {
  committed_memory_ = 0;
}

SystemMetrics SystemMetrics::Sample() {
  SystemMetrics system_metrics;

  system_metrics.committed_memory_ = GetSystemCommitCharge();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  GetSystemMemoryInfo(&system_metrics.memory_info_);
  GetVmStatInfo(&system_metrics.vmstat_info_);
  GetSystemDiskInfo(&system_metrics.disk_info_);
#endif
#if BUILDFLAG(IS_CHROMEOS)
  GetSwapInfo(&system_metrics.swap_info_);
  GetGraphicsMemoryInfo(&system_metrics.gpu_memory_info_);
#endif
#if BUILDFLAG(IS_WIN)
  GetSystemPerformanceInfo(&system_metrics.performance_);
#endif
  return system_metrics;
}

Value::Dict SystemMetrics::ToDict() const {
  Value::Dict res;

  res.Set("committed_memory", static_cast<int>(committed_memory_));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  Value::Dict meminfo = memory_info_.ToDict();
  meminfo.Merge(vmstat_info_.ToDict());
  res.Set("meminfo", std::move(meminfo));
  res.Set("diskinfo", disk_info_.ToDict());
#endif
#if BUILDFLAG(IS_CHROMEOS)
  res.Set("swapinfo", swap_info_.ToDict());
  res.Set("gpu_meminfo", gpu_memory_info_.ToDict());
#endif
#if BUILDFLAG(IS_WIN)
  res.Set("perfinfo", performance_.ToDict());
#endif

  return res;
}

ProcessMetrics::~ProcessMetrics() = default;

std::unique_ptr<ProcessMetrics> ProcessMetrics::CreateCurrentProcessMetrics() {
#if !BUILDFLAG(IS_MAC)
  return CreateProcessMetrics(base::GetCurrentProcessHandle());
#else
  return CreateProcessMetrics(base::GetCurrentProcessHandle(), nullptr);
#endif  // !BUILDFLAG(IS_MAC)
}

#if !BUILDFLAG(IS_FREEBSD) || !BUILDFLAG(IS_POSIX)
double ProcessMetrics::GetPlatformIndependentCPUUsage(
    TimeDelta cumulative_cpu) {
  TimeTicks time = TimeTicks::Now();

  if (last_cumulative_cpu_.is_zero()) {
    // First call, just set the last values.
    last_cumulative_cpu_ = cumulative_cpu;
    last_cpu_time_ = time;
    return 0;
  }

  TimeDelta cpu_time_delta = cumulative_cpu - last_cumulative_cpu_;
  TimeDelta time_delta = time - last_cpu_time_;
  if (time_delta.is_zero())
    return 0;

  last_cumulative_cpu_ = cumulative_cpu;
  last_cpu_time_ = time;

  return 100.0 * cpu_time_delta / time_delta;
}

base::expected<double, ProcessCPUUsageError>
ProcessMetrics::GetPlatformIndependentCPUUsage() {
  return GetCumulativeCPUUsage().transform([this](base::TimeDelta cpu_usage) {
    return GetPlatformIndependentCPUUsage(cpu_usage);
  });
}
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_AIX)
int ProcessMetrics::CalculateIdleWakeupsPerSecond(
    uint64_t absolute_idle_wakeups) {
  return CalculateEventsPerSecond(absolute_idle_wakeups,
                                  &last_absolute_idle_wakeups_,
                                  &last_idle_wakeups_time_);
}
#else
int ProcessMetrics::GetIdleWakeupsPerSecond() {
  NOTIMPLEMENTED();  // http://crbug.com/120488
  return 0;
}
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
        // || BUILDFLAG(IS_AIX)

#if BUILDFLAG(IS_APPLE)
int ProcessMetrics::CalculatePackageIdleWakeupsPerSecond(
    uint64_t absolute_package_idle_wakeups) {
  return CalculateEventsPerSecond(absolute_package_idle_wakeups,
                                  &last_absolute_package_idle_wakeups_,
                                  &last_package_idle_wakeups_time_);
}

#endif  // BUILDFLAG(IS_APPLE)

}  // namespace base
