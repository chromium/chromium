// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock_metrics_recorder.h"

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/ring_buffer.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/platform_thread_ref.h"
#include "base/time/time.h"

namespace base {

namespace {

// The histogram bounds (1us to 1s) are chosen to select for meaningful lock
// contention. A lower bound of 1us (the approximate maximum overhead of a
// no-op syscall) helps filter out noise from uncontended lock acquisitions
// including cases where the detection of contention is best-effort, such as
// in Lock. An upper bound of 1s is used because waits longer than this
// are exceptionally rare, and bucketing them together is sufficient for
// analysis.
void ReportBaseLockHistogram(const TimeDelta& sample) {
  static const NoDestructor<const std::string> base_lock_hist_name(
      StrCat({"Scheduling.ContendedLockAcquisitionTime.BaseLock.",
              PlatformThread::GetName()}));
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(*base_lock_hist_name, sample,
                                          Microseconds(1), Seconds(1), 100);
}

void ReportPartitionAllocLockHistogram(const TimeDelta& sample) {
  static const NoDestructor<const std::string> pa_lock_hist_name(
      StrCat({"Scheduling.ContendedLockAcquisitionTime.PartitionAllocLock.",
              PlatformThread::GetName()}));
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(*pa_lock_hist_name, sample,
                                          Microseconds(1), Seconds(1), 100);
}

}  // namespace

void LockMetricsRecorder::RecordLockAcquisitionTime(TimeDelta sample,
                                                    LockType type) {
  buffer_[static_cast<size_t>(type)].SaveToBuffer(sample);
}

void LockMetricsRecorder::ForEachSample(LockType type,
                                        FunctionRef<void(const TimeDelta&)> f) {
  CHECK(!iterating_in_progress_);
  CHECK_LE(type, LockType::kMax);
  // Set the `reporting_in_progress_` flag to true to prevent re-entrancy due to
  // any lock contention during the recording of the histogram. This keeps the
  // recording and reporting logic simple at the cost of a tiny blind-spot in
  // our metrics.
  AutoReset<bool> mark_iterating_in_progress(&iterating_in_progress_, true);
  auto& buffer = buffer_[static_cast<size_t>(type)];
  for (auto it = buffer.Begin(); it; ++it) {
    f(**it);
  }
  buffer.Clear();
}

void LockMetricsRecorder::ReportLockAcquisitionTimes() {
  // Only report samples from target thread.
  if (!IsCurrentThreadTarget()) {
    return;
  }

  ForEachSample(LockType::kBaseLock, ReportBaseLockHistogram);
  ForEachSample(LockType::kPartitionAllocLock,
                ReportPartitionAllocLockHistogram);
}

// static
void LockMetricsRecorder::SetTargetCurrentThread() {
  PlatformThreadRef current_thread = PlatformThread::CurrentRef();
  PlatformThreadRef prev_target_thread =
      target_thread_.exchange(current_thread, std::memory_order_relaxed);
  CHECK(prev_target_thread.is_null() || prev_target_thread == current_thread);
}

// static
LockMetricsRecorder* LockMetricsRecorder::Get() {
  static LockMetricsRecorder g_lock_metrics_recorder;
  return &g_lock_metrics_recorder;
}

// static
LockMetricsRecorder::ScopedLockAcquisitionTimer
LockMetricsRecorder::ScopedLockAcquisitionTimer::CreateForTest(
    LockMetricsRecorder* recorder) {
  return LockMetricsRecorder::ScopedLockAcquisitionTimer(recorder);
}

}  // namespace base
