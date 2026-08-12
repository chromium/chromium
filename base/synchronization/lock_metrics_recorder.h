// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_LOCK_METRICS_RECORDER_H_
#define BASE_SYNCHRONIZATION_LOCK_METRICS_RECORDER_H_

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
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

// A tag that can be used to apply unique identifiers to a lock.
//
// Creation:
// Instances must be created as `static constinit` objects with a unique name
// for each different lock type, as the tag name is used as part of the suffix
// in UMA histogram names. It is recommended to create a static function getter
// for the `LockMetricTag`, and use the same getter throughout the subsystem:
//   const base::LockMetricTag& GetMyFeatureLockMetricTag() {
//     static constinit base::LockMetricTag tag("MyFeatureLock");
//     return tag;
//   }
//
// Usage:
// Pass a `LockMetricTag` to `ScopedLockAcquisitionTimer` prior to
// acquiring a lock. The acquisition duration will be recorded in the histogram
// associated with that tag.
//
// Example usage:
//     base::LockMetricsRecorder::ScopedLockAcquisitionTimer timer(
//         GetMyFeatureLockMetricTag());
//
// If creating a new `LockMetricTag` in a subsystem, add a histogram
// in the appropriate histograms.xml file to capture the lock acquisition times.

class BASE_EXPORT LockMetricTag {
 public:
  LockMetricTag(const LockMetricTag&) = delete;
  LockMetricTag& operator=(const LockMetricTag&) = delete;
  ~LockMetricTag() = default;

  consteval explicit LockMetricTag(std::string_view name)
      : name_(name), hash_(HashName(name)) {}

  constexpr std::string_view name() const { return name_; }
  constexpr uint64_t hash() const { return hash_; }

 private:
  // Generates a precomputed 64-bit hash key for tag names using FNV-1a hashing.
  // Standard Chromium hashing helpers such as `base::FastHash` and
  // `base::PersistentHash` are non-constexpr and cannot be evaluated in
  // consteval constructors required for `LockMetricTag` instances.
  static constexpr uint64_t HashName(std::string_view name) {
    constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
    constexpr uint64_t kFnvPrime = 1099511628211ULL;

    uint64_t hash = kFnvOffsetBasis;
    for (char c : name) {
      hash ^= static_cast<uint8_t>(c);
      hash *= kFnvPrime;
    }
    return hash;
  }

  const std::string_view name_;
  const uint64_t hash_;
};

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

  // Samples of lock acquisition times and types stored in a ring buffer.
  struct LockMetricSample {
    base::TimeDelta wait_time;
    raw_ptr<const LockMetricTag> lock_type;
  };

  // The internal buffer size is a trade-off between memory usage and the number
  // of samples that can be stored. With sampling, this buffer size should be
  // sufficient for most cases. If the buffer overflows, the `RingBuffer` will
  // overwrite the oldest samples.
  constexpr static size_t kMaxSamples = 512;

  explicit LockMetricsRecorder(PassKey, std::string_view histogram_suffix);
  LockMetricsRecorder(const LockMetricsRecorder&) = delete;
  LockMetricsRecorder& operator=(const LockMetricsRecorder&) = delete;
  ~LockMetricsRecorder();

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
    // Constructs a timer to measure lock acquisition duration.
    //
    // `tagged_lock_type`: The metric tag identifying the lock type. Must be a
    // valid reference (non-optional). The caller is responsible for ensuring
    // that the lifetime of `tagged_lock_type` outlives the
    // `ScopedLockAcquisitionTimer` object and any recorded samples until they
    // are reported.
    explicit ScopedLockAcquisitionTimer(const LockMetricTag& tagged_lock_type)
        : ScopedLockAcquisitionTimer(LockMetricsRecorder::GetForCurrentThread(),
                                     tagged_lock_type) {}

    ScopedLockAcquisitionTimer(const ScopedLockAcquisitionTimer&) = delete;
    ScopedLockAcquisitionTimer& operator=(const ScopedLockAcquisitionTimer&) =
        delete;

    ~ScopedLockAcquisitionTimer() {
      if (!start_time_.has_value()) [[likely]] {
        return;
      }

      lock_metrics_->RecordLockAcquisitionTime(LockMetricSample{
          subtle::TimeTicksNowIgnoringOverride() - *start_time_,
          tagged_lock_type_});
    }

    static ScopedLockAcquisitionTimer CreateForTest(
        LockMetricsRecorder* lock_metrics,
        const LockMetricTag& tagged_lock_type);

   private:
    ScopedLockAcquisitionTimer(LockMetricsRecorder* lock_metrics,
                               const LockMetricTag& tagged_lock_type)
        : lock_metrics_(lock_metrics), tagged_lock_type_(&tagged_lock_type) {
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

    // The caller must ensure `tagged_lock_type_` outlives this timer and any
    // samples recorded until reported.
    const LockMetricTag* tagged_lock_type_;
  };

 private:
  bool CalledOnValidThread() const {
    return bound_thread_ref_ == base::PlatformThread::CurrentRef();
  }

  constexpr static double kSamplingRatio = 0.001;

  const std::string histogram_suffix_;

  base::HistogramBase* GetOrCreateHistogram(const LockMetricTag* lock_tag);

  void ReportLockHistogram(const LockMetricSample& sample);

  bool iterating_in_progress_ = false;

  RingBuffer<LockMetricSample, kMaxSamples> unified_sample_buffer_;

  // Include the subsampler in the thread-local data to avoid reallocations
  // when the subsampler is created and destroyed.
  MetricsSubSampler subsampler_;

  // Maps `LockMetricTag` hashes to UMA histogram pointers for this thread.
  base::flat_map<uint64_t, base::HistogramBase*> tagged_lock_histograms_;

  // The thread ref of the thread that created this object.
  //
  // Note: `DCHECK_CALLED_ON_VALID_THREAD` cannot be used as it may allocate
  // memory and cause infinite recursion depending on TLS teardown order.
  const base::PlatformThreadRef bound_thread_ref_ =
      base::PlatformThread::CurrentRef();
};

}  // namespace base

#endif  // BASE_SYNCHRONIZATION_LOCK_METRICS_RECORDER_H_
