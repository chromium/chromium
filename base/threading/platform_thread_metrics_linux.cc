// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread_metrics.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/numerics/clamped_math.h"
#include "base/process/internal_linux.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"

namespace base {

namespace {

// Get the total CPU from a proc stat buffer. Return value is a TimeDelta
// converted from a number of jiffies on success or nullopt if parsing failed.
std::optional<TimeDelta> ParseTotalCPUTimeFromStats(
    span<std::string_view> proc_stats) {
  const std::optional<int64_t> utime =
      internal::GetProcStatsFieldAsOptionalInt64(proc_stats,
                                                 internal::VM_UTIME);
  if (utime.value_or(-1) < 0) {
    return std::nullopt;
  }
  const std::optional<int64_t> stime =
      internal::GetProcStatsFieldAsOptionalInt64(proc_stats,
                                                 internal::VM_STIME);
  if (stime.value_or(-1) < 0) {
    return std::nullopt;
  }
  const TimeDelta cpu_time =
      internal::ClockTicksToTimeDelta(ClampAdd(utime.value(), stime.value()));
  CHECK(!cpu_time.is_negative());
  return cpu_time;
}

}  // namespace

std::optional<TimeDelta> PlatformThreadMetrics::GetCumulativeCPUUsage() {
  TRACE_EVENT("base", "Thread::GetCumulativeCPUUsage");
  const auto thread_stat_path = FilePath(internal::kProcDir)
                                    .Append(NumberToString(tid_.raw()))
                                    .Append(internal::kStatFile);
  std::string buffer;
  std::vector<std::string_view> proc_stats;
  if (!internal::ReadProcFile(thread_stat_path, &buffer) ||
      !internal::ParseProcStats(buffer, &proc_stats)) {
    return std::nullopt;
  }
  return ParseTotalCPUTimeFromStats(proc_stats);
}

}  // namespace base
