// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread_metrics.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {

// Apple and Windows have platform-specific create methods.

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)

// static
std::unique_ptr<PlatformThreadMetrics>
PlatformThreadMetrics::CreateForCurrentThread() {
  return CreateFromHandle(PlatformThread::CurrentHandle());
}

#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)

// static
std::unique_ptr<PlatformThreadMetrics>
PlatformThreadMetrics::CreateForCurrentThread() {
  return CreateFromId(PlatformThread::CurrentId());
}

// static
std::unique_ptr<PlatformThreadMetrics> PlatformThreadMetrics::CreateFromId(
    PlatformThreadId tid) {
  if (tid == kInvalidThreadId) {
    return nullptr;
  }
  return WrapUnique(new PlatformThreadMetrics(tid));
}

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)

PlatformThreadMetrics::~PlatformThreadMetrics() = default;

double PlatformThreadMetrics::GetCPUUsageProportion(TimeDelta cumulative_cpu) {
  TimeTicks time = TimeTicks::Now();

  if (!last_cpu_time_.has_value()) {
    // First call, just set the last values.
    CHECK(last_cumulative_cpu_.is_zero());
    last_cumulative_cpu_ = cumulative_cpu;
    last_cpu_time_ = time;
    return 0;
  }

  TimeDelta cpu_time_delta = cumulative_cpu - last_cumulative_cpu_;
  TimeDelta time_delta = time - last_cpu_time_.value();
  if (time_delta.is_zero()) {
    return 0;
  }

  last_cumulative_cpu_ = cumulative_cpu;
  last_cpu_time_ = time;

  return 100.0 * cpu_time_delta / time_delta;
}

std::optional<double> PlatformThreadMetrics::GetCPUUsageProportion() {
  if (const std::optional<TimeDelta> cpu_usage = GetCumulativeCPUUsage()) {
    return GetCPUUsageProportion(cpu_usage.value());
  }
  return std::nullopt;
}

}  // namespace base
