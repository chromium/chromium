// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_PLATFORM_THREAD_METRICS_H_
#define BASE_THREADING_PLATFORM_THREAD_METRICS_H_

#include <memory>
#include <optional>

#include "base/base_export.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace base {

// Used to log metrics about a specific thread. Modeled on ProcessMetrics.
//
// On Android and Fuchsia the thread must belong to the current process. (This
// is due to the way Chrome spawns processes on Android, so uses of
// PlatformThreadMetrics outside of Chrome might not have this restriction.)
//
// CPU usage of the current thread can also be measured with base::ThreadTicks.
// However on most platforms ThreadTicks can't measure other threads.
//
// Depending on platform, measuring CPU usage needs either a thread ID (TID) or
// a thread handle:
//
// * Apple needs a thread handle, and there's no obvious way to convert a TID to
//   a handle, so it only supports CreateFromHandle().
// * Windows needs a thread handle, but it's possible to convert back and forth
//   between TID's and handles, so it supports both CreateFromHandle() and
//   CreateFromId().
// * Every other platform needs a TID, and there's no obvious way to convert a
//   handle to a TID, so they only support CreateFromId().
//
// TODO(crbug.com/420681350): There might be ways to support CreateFromHandle()
// and threads in other processes on Fuchsia. Improve the Fuchsia implementation
// if there's any demand.
class BASE_EXPORT PlatformThreadMetrics {
 public:
  static std::unique_ptr<PlatformThreadMetrics> CreateForCurrentThread();

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  static std::unique_ptr<PlatformThreadMetrics> CreateFromHandle(
      PlatformThreadHandle handle);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  static std::unique_ptr<PlatformThreadMetrics> CreateFromId(
      PlatformThreadId tid);
#endif

  PlatformThreadMetrics(const PlatformThreadMetrics&) = delete;
  PlatformThreadMetrics& operator=(const PlatformThreadMetrics&) = delete;

  ~PlatformThreadMetrics();

  // Returns the cumulative CPU usage of the thread since process start, or
  // nullopt on error. Unlike ProcessMetrics::GetCumulativeCPUUsage() this
  // doesn't measure multiple concurrent threads, so it shouldn't return more
  // than 1 second of CPU utilization per 1 wall-clock second. However due to
  // rounding it may report slightly higher values under heavy load.
  std::optional<TimeDelta> GetCumulativeCPUUsage();

  // Returns the percentage of time spent executing the thread in the interval
  // since the last time the method was called, using the current
  // `cumulative_cpu`. Equivalent to ProcessMetrics::
  // GetPlatformIndependentCPUUsage(), but this doesn't measure multiple
  // concurrent threads, so it should always return a value in the range 0% to
  // 100%. However due to rounding it may report slightly more than 100% under
  // heavy load.
  //
  // Since this API measures usage over an interval, it will return zero on the
  // first call, and an actual value only on the second and subsequent calls.
  double GetCPUUsageProportion(TimeDelta cumulative_cpu);

  // Same as the above, but automatically calls GetCumulativeCPUUsage() to
  // determine the current cumulative CPU. Returns nullopt if
  // GetCumulativeCPUUsage() fails.
  std::optional<double> GetCPUUsageProportion();

 private:
#if BUILDFLAG(IS_APPLE)
  explicit PlatformThreadMetrics(PlatformThreadHandle handle)
      : handle_(handle) {}

  PlatformThreadHandle handle_;
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)
  explicit PlatformThreadMetrics(PlatformThreadId tid) : tid_(tid) {}

  PlatformThreadId tid_;
#elif BUILDFLAG(IS_WIN)
  explicit PlatformThreadMetrics(win::ScopedHandle&& handle);

  win::ScopedHandle handle_;
#endif

  // Previous measurements used to compute the CPU usage between calls to
  // GetCPUUsageProportion().
  std::optional<TimeTicks> last_cpu_time_;
  TimeDelta last_cumulative_cpu_;
};

}  // namespace base

#endif  // BASE_THREADING_PLATFORM_THREAD_METRICS_H_
