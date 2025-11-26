// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread_metrics.h"

#include <memory>
#include <mutex>
#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/sparse_histogram.h"
#include "base/no_destructor.h"
#include "base/process/current_process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/threading/platform_thread_internal_posix.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace base {

namespace {

#if BUILDFLAG(IS_ANDROID)
std::string GetCurrentProcessName(CurrentProcess::NameKey key) {
  std::string process_name = CurrentProcess::GetInstance().GetName(key);
  // The process name may have whitespaces (e.g. 'GPU Process') and that won't
  // work as a histogram name.
  RemoveChars(process_name, " ", &process_name);
  return process_name;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

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

#if BUILDFLAG(IS_ANDROID)

PlatformThreadPriorityMonitor::PlatformThreadPriorityMonitor()
    : process_name_(GetCurrentProcessName({})) {}
PlatformThreadPriorityMonitor::~PlatformThreadPriorityMonitor() = default;

// static
PlatformThreadPriorityMonitor& PlatformThreadPriorityMonitor::Get() {
  static base::NoDestructor<PlatformThreadPriorityMonitor> instance;
  return *instance;
}

std::string PlatformThreadPriorityMonitor::GetHistogramNameForSuffix(
    const std::string_view suffix) {
  return StrCat({"Scheduling.ThreadPriority.", process_name_, ".", suffix});
}

void PlatformThreadPriorityMonitor::RegisterCurrentThread(
    const std::string_view suffix) {
  PlatformThreadId current_thread_id = PlatformThread::CurrentId();
  HistogramBase* histogram =
      SparseHistogram::FactoryGet(GetHistogramNameForSuffix(suffix),
                                  HistogramBase::kUmaTargetedHistogramFlag);

  AutoLock auto_lock(lock_);
  thread_id_to_histogram_[current_thread_id] = histogram;
}

void PlatformThreadPriorityMonitor::UnregisterCurrentThread() {
  PlatformThreadId current_thread_id = PlatformThread::CurrentId();

  AutoLock auto_lock(lock_);
  thread_id_to_histogram_.erase(current_thread_id);
}

void PlatformThreadPriorityMonitor::RecordThreadPriorities() {
  AutoLock auto_lock(lock_);
  for (auto& [thread_id, histogram] : thread_id_to_histogram_) {
    histogram->Add(internal::GetThreadNiceValue(thread_id));
  }
}

// static
void PlatformThreadPriorityMonitor::ScheduleRecordingTask() {
  static NoDestructor<RepeatingClosure> closure(BindRepeating([] {
    auto& monitor = PlatformThreadPriorityMonitor::Get();
    monitor.RecordThreadPriorities();
    monitor.ScheduleRecordingTask();
  }));

  ThreadPool::PostDelayedTask(
      FROM_HERE,
      {TaskPriority::BEST_EFFORT, TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      *closure, kMinSamplingInterval);
}

void PlatformThreadPriorityMonitor::Start() {
  std::call_once(once_flag_,
                 &PlatformThreadPriorityMonitor::ScheduleRecordingTask);
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace base
