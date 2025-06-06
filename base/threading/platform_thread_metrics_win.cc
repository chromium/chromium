// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread_metrics.h"

#include <windows.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_handle.h"
#include "build/build_config.h"

namespace base {

namespace {

std::optional<TimeDelta> GetImpreciseCumulativeCPUUsage(
    const win::ScopedHandle& handle) {
  FILETIME creation_time;
  FILETIME exit_time;
  FILETIME kernel_time;
  FILETIME user_time;

  if (!::GetThreadTimes(handle.get(), &creation_time, &exit_time, &kernel_time,
                        &user_time)) {
    return std::nullopt;
  }

  return TimeDelta::FromFileTime(kernel_time) +
         TimeDelta::FromFileTime(user_time);
}

}  // namespace

// static
std::unique_ptr<PlatformThreadMetrics> PlatformThreadMetrics::CreateFromHandle(
    PlatformThreadHandle handle) {
  // Duplicate the handle to get one that can be owned by PlatformThreadMetrics.
  HANDLE new_handle = INVALID_HANDLE_VALUE;
  if (!::DuplicateHandle(/*hSourceProcessHandle=*/::GetCurrentProcess(),
                         /*hSourceHandle=*/handle.platform_handle(),
                         /*hTargetProcessHandle=*/::GetCurrentProcess(),
                         &new_handle, THREAD_QUERY_LIMITED_INFORMATION,
                         /*bInheritHandle=*/FALSE, /*dwOptions=*/0)) {
    return nullptr;
  }
  return WrapUnique(new PlatformThreadMetrics(win::ScopedHandle(new_handle)));
}

// static
std::unique_ptr<PlatformThreadMetrics> PlatformThreadMetrics::CreateFromId(
    PlatformThreadId tid) {
  if (tid == kInvalidThreadId) {
    return nullptr;
  }
  win::ScopedHandle handle(::OpenThread(THREAD_QUERY_LIMITED_INFORMATION,
                                        /*bInheritHandle=*/FALSE, tid.raw()));
  if (!handle.is_valid()) {
    return nullptr;
  }

  return WrapUnique(new PlatformThreadMetrics(std::move(handle)));
}

PlatformThreadMetrics::PlatformThreadMetrics(win::ScopedHandle&& handle)
    : handle_(std::move(handle)) {}

std::optional<TimeDelta> PlatformThreadMetrics::GetCumulativeCPUUsage() {
  TRACE_EVENT("base", "Thread::GetCumulativeCPUUsage");
#if defined(ARCH_CPU_ARM64)
  // Precise CPU usage is not available on Arm CPUs because they don't support
  // constant rate TSC.
  return GetImpreciseCumulativeCPUUsage(handle_);
#else   // !defined(ARCH_CPU_ARM64)
  if (!time_internal::HasConstantRateTSC()) {
    return GetImpreciseCumulativeCPUUsage(handle_);
  }

  const double tsc_ticks_per_second = time_internal::TSCTicksPerSecond();
  if (tsc_ticks_per_second == 0) {
    // TSC is only initialized once TSCTicksPerSecond() is called twice 50 ms
    // apart on the same thread to get a baseline. In unit tests, it is frequent
    // for the initialization not to be complete. In production, it can also
    // theoretically happen.
    return GetImpreciseCumulativeCPUUsage(handle_);
  }

  ULONG64 cycle_time = 0;
  if (!::QueryThreadCycleTime(handle_.get(), &cycle_time)) {
    return std::nullopt;
  }

  return Seconds(cycle_time / tsc_ticks_per_second);
#endif  // !defined(ARCH_CPU_ARM64)
}

}  // namespace base
