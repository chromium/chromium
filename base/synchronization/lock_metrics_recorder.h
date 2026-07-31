// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_LOCK_METRICS_RECORDER_H_
#define BASE_SYNCHRONIZATION_LOCK_METRICS_RECORDER_H_

#include <atomic>
#include <cstddef>
#include <optional>
#include <string_view>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/containers/ring_buffer.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/rand_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/types/pass_key.h"

namespace base {

class HistogramBase;

// This class is a thread-local object that uses TLS to store samples of metrics
// related to locks, which are then reported to UMA histograms when the thread
// goes idle.
//
// - `GetForCurrentThread()` is thread-safe.
// - To prevent reentrancy and deadlocks, recording metrics and subsampling
//   does not allocate memory or acquire any locks (see .cc file).

class BASE_EXPORT LockMetricsRecorder {
 public:
  using PassKey =
      base::PassKey<LockMetricsRecorder, class LockMetricsRecorderTest>;

  // The type of lock the sample is associated with.
  enum class LockType : size_t {
    // For samples associated with base::Lock
    kBaseLock = 0,
    // For samples associated with partition_alloc::internal::Lock
    kPartitionAllocLock = 1,
  };

  // Samples of lock acquisition times and types stored in a ring buffer.
  struct LockMetricSample {
    base::TimeDelta wait_time;
    LockType type;
  };

  // The internal buffer size is a trade-off between memory usage and the number
  // of samples that can be stored. With sampling, this buffer size should be
  // sufficient for most cases. If the buffer overflows, the `RingBuffer` will
  // overwrite the oldest samples.
  constexpr static size_t kMaxSamples = 512;

  explicit LockMetricsRecorder(PassKey, std::string_view histogram_suffix);
  LockMetricsRecorder(const LockMetricsRecorder&) = delete;
  LockMetricsRecorder& operator=(const LockMetricsRecorder&) = delete;
  ~LockMetricsRecorder() = default;

  // Get the thread-local instance of the lock metrics recorder.
  // Returns nullptr if `EnableRecordingOnCurrentThread()` was not called for this
  // thread.
  static LockMetricsRecorder* GetForCurrentThread();

  static void EnableRecordingOnCurrentThread(std::string_view histogram_suffix);

  static void DisableRecordingOnCurrentThreadForTesting();

  static void SetAllowedThreadsForTesting(
      std::vector<std::string> allowed_threads);

  bool ShouldRecordLockAcquisitionTime() const;

  // Records a `LockMetricSample` into the internal buffer. Must be called on
  // the target thread.
  void RecordLockAcquisitionTime(const LockMetricSample& sample);

  // Report lock acquisition times to UMA histograms, if the current thread is
  // the target thread.
  void ReportLockAcquisitionTimes();

  // Iterate over each `LockMetricSample` in the unified buffer and
  // synchronously call the FunctionRef for each sample. Only exposed for
  // testing. Call `ReportLockAcquisitionTimes()` to report histograms for all
  // the stored samples.
  void ForEachSample(FunctionRef<void(const LockMetricSample&)> f);

  // Timer that records into a lock metrics object.
  class BASE_EXPORT ScopedLockAcquisitionTimer {
    STACK_ALLOCATED();

   public:
    ScopedLockAcquisitionTimer()
        : ScopedLockAcquisitionTimer(
              LockMetricsRecorder::GetForCurrentThread()) {}

    ScopedLockAcquisitionTimer(const ScopedLockAcquisitionTimer&) = delete;
    ScopedLockAcquisitionTimer& operator=(const ScopedLockAcquisitionTimer&) =
        delete;

    ~ScopedLockAcquisitionTimer() {
      if (!start_time_.has_value()) [[likely]] {
        return;
      }

      lock_metrics_->RecordLockAcquisitionTime(LockMetricSample{
          subtle::TimeTicksNowIgnoringOverride() - *start_time_,
          LockType::kBaseLock});
    }

    static ScopedLockAcquisitionTimer CreateForTest(
        LockMetricsRecorder* lock_metrics);

   private:
    explicit ScopedLockAcquisitionTimer(LockMetricsRecorder* lock_metrics)
        : lock_metrics_(lock_metrics) {
      if (!lock_metrics_ || !lock_metrics_->ShouldRecordLockAcquisitionTime())
          [[likely]] {
        return;
      }

      start_time_.emplace(subtle::TimeTicksNowIgnoringOverride());
    }

    // `ElapsedTimer` is not used here since it is mocked in tests and the mock
    // might acquire a base::Lock thereby causing re-entrancy.
    std::optional<TimeTicks> start_time_;

    // It is safe to hold onto the pointer to the lock metrics recorder since
    // it points to a thread-local variable.
    const raw_ptr<LockMetricsRecorder> lock_metrics_;
  };

 private:
  bool CalledOnValidThread() const {
    return bound_thread_ref_ == base::PlatformThread::CurrentRef();
  }

  constexpr static double kSamplingRatio = 0.001;

  static void ReportLockHistogram(TimeDelta sample,
                                  base::HistogramBase* histogram_pointer);

  bool iterating_in_progress_ = false;

  raw_ptr<base::HistogramBase> base_lock_histogram_ = nullptr;
  raw_ptr<base::HistogramBase> partition_alloc_lock_histogram_ = nullptr;

  RingBuffer<LockMetricSample, kMaxSamples> unified_sample_buffer_;

  // Include the subsampler in the thread-local data to avoid reallocations
  // when the subsampler is created and destroyed.
  MetricsSubSampler subsampler_;

  // The thread ref of the thread that created this object.
  //
  // Note: `DCHECK_CALLED_ON_VALID_THREAD` cannot be used as it may allocate
  // memory and cause infinite recursion depending on TLS teardown order.
  const base::PlatformThreadRef bound_thread_ref_ =
      base::PlatformThread::CurrentRef();
};

}  // namespace base

#endif  // BASE_SYNCHRONIZATION_LOCK_METRICS_RECORDER_H_
