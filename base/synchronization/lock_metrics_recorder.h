// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_LOCK_METRICS_RECORDER_H_
#define BASE_SYNCHRONIZATION_LOCK_METRICS_RECORDER_H_

#include <atomic>
#include <cstddef>
#include <optional>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/containers/ring_buffer.h"
#include "base/functional/function_ref.h"
#include "base/memory/stack_allocated.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/time/time_override.h"

namespace base {

// A class used to hold samples of metrics related to locks.
//
// `LockMetricsRecorder` is not thread-safe and all samples must be recorded
// from the same thread for the lifetime of the process after setting that
// thread as the target thread.
class BASE_EXPORT LockMetricsRecorder {
 public:
  // The internal buffer size is a trade-off between memory usage and the number
  // of samples that can be stored. With sampling, this buffer size should be
  // sufficient for most cases. If the buffer overflows, the RingBuffer will
  // overwrite the oldest samples.
  constexpr static size_t kMaxSamples = 256;

  LockMetricsRecorder() = default;
  LockMetricsRecorder(const LockMetricsRecorder&) = delete;
  LockMetricsRecorder& operator=(const LockMetricsRecorder&) = delete;
  ~LockMetricsRecorder() = default;

  // Get the global instance of lock metrics recorder.
  static LockMetricsRecorder* Get();

  // Sets current thread as the target thread for recording lock-related
  // metrics.
  //
  // This function can be called multiple times from the same thread but will
  // crash if called from different threads.
  void SetTargetCurrentThread();

  inline bool IsCurrentThreadTarget() const {
    return target_thread_.load(std::memory_order_relaxed) ==
           PlatformThread::CurrentRef();
  }

  inline bool ShouldRecordLockAcquisitionTime() const {
    return IsCurrentThreadTarget() && !iterating_in_progress_ &&
           metrics_sub_sampler_.ShouldSample(kSamplingRatio);
  }

  // The type of lock the sample is associated with.
  enum class LockType : size_t {
    // For samples associated with base::Lock
    kBaseLock = 0,
    // For samples associated with partition_alloc::internal::Lock
    kPartitionAllocLock = 1,
    kMax = kPartitionAllocLock,
  };

  // Records a sample into the internal buffer. Must be called on the target
  // thread.
  void RecordLockAcquisitionTime(TimeDelta sample, LockType type);

  // Report lock acquisition times to UMA histograms, if the current thread is
  // the target thread.
  void ReportLockAcquisitionTimes();

  // Iterate over all the samples of the given type and synchronously call the
  // FunctionRef for each sample. Only exposed for testing. Call
  // `ReportLockAcquisitionTimes()` to report histograms for all the stored
  // samples.
  void ForEachSample(LockType type, FunctionRef<void(const TimeDelta&)> f);

  // Timer that records into a lock metrics object.
  class BASE_EXPORT ScopedLockAcquisitionTimer {
    STACK_ALLOCATED();

   public:
    ScopedLockAcquisitionTimer()
        : ScopedLockAcquisitionTimer(LockMetricsRecorder::Get()) {}

    ScopedLockAcquisitionTimer(const ScopedLockAcquisitionTimer&) = delete;
    ScopedLockAcquisitionTimer& operator=(const ScopedLockAcquisitionTimer&) =
        delete;

    ~ScopedLockAcquisitionTimer() {
      if (!start_time_.has_value()) [[likely]] {
        return;
      }

      lock_metrics_->RecordLockAcquisitionTime(
          subtle::TimeTicksNowIgnoringOverride() - *start_time_,
          LockType::kBaseLock);
    }

    static ScopedLockAcquisitionTimer CreateForTest(
        LockMetricsRecorder* lock_metrics);

   private:
    explicit ScopedLockAcquisitionTimer(LockMetricsRecorder* lock_metrics)
        : lock_metrics_(lock_metrics) {
      if (!lock_metrics_->ShouldRecordLockAcquisitionTime()) [[likely]] {
        return;
      }

      start_time_.emplace(subtle::TimeTicksNowIgnoringOverride());
    }

    // `ElapsedTimer` is not used here since it is mocked in tests and the mock
    // might acquire a base::Lock thereby causing re-entrancy.
    std::optional<TimeTicks> start_time_;

    // It is safe to hold onto the pointer to the lock metrics recorder since
    // it points to a global variable.
    const raw_ptr<LockMetricsRecorder> lock_metrics_;
  };

 private:
  constexpr static double kSamplingRatio = 0.001;
  std::array<RingBuffer<TimeDelta, kMaxSamples>,
             static_cast<size_t>(LockType::kMax) + 1>
      buffer_;
  MetricsSubSampler metrics_sub_sampler_;
  bool iterating_in_progress_ = false;
  // Thread local variables on Android are extremely slow. So on the hot-path,
  // use atomics to record the target thread-ref and read it back from multiple
  // threads, adding the constraints that samples can be recorded only from the
  // same thread for the lifetime of the process.
  std::atomic<PlatformThreadRef> target_thread_;
};

}  // namespace base

#endif  // BASE_SYNCHRONIZATION_LOCK_METRICS_RECORDER_H_
