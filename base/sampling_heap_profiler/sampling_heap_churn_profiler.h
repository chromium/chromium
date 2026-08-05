// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SAMPLING_HEAP_PROFILER_SAMPLING_HEAP_CHURN_PROFILER_H_
#define BASE_SAMPLING_HEAP_PROFILER_SAMPLING_HEAP_CHURN_PROFILER_H_

#include <atomic>
#include <vector>

#include "base/base_export.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace base {

// The class implements sampling profiling of alloc-free memory churn.
// It is called directly by SamplingHeapProfiler when a sampled allocation
// is freed, and records a subsampled stream of alloc-free stack traces.
class BASE_EXPORT SamplingHeapChurnProfiler {
 public:
  SamplingHeapChurnProfiler(const SamplingHeapChurnProfiler&) = delete;
  SamplingHeapChurnProfiler& operator=(const SamplingHeapChurnProfiler&) =
      delete;

  // Starts collecting churn samples.
  void Start();
  void Stop();

  void SetSubsamplingChance(double chance);

  // Returns true if an alloc-free event should be recorded based on the
  // configured subsampling chance. Called by SamplingHeapProfiler before moving
  // the sample.
  bool ShouldRecordAllocFree() const;

  // Returns and clears all accumulated churn samples.
  std::vector<SamplingHeapProfiler::Sample> TakeSamples();

  // Called directly by SamplingHeapProfiler when a live sample is removed.
  void RecordAllocFree(SamplingHeapProfiler::Sample sample);

 private:
  SamplingHeapChurnProfiler();
  ~SamplingHeapChurnProfiler();

  friend class SamplingHeapProfiler;
  friend struct std::default_delete<SamplingHeapChurnProfiler>;

  // Mutex to guard access to |churn_samples_|.
  Lock mutex_;
  std::vector<SamplingHeapProfiler::Sample> churn_samples_ GUARDED_BY(mutex_);
  double subsampling_chance_ = 0.005;

  // Number of running profiling sessions. Stored as an atomic to allow a fast,
  // lock-free check on the hot path when any sampled allocation is freed.
  // Relaxed memory ordering is sufficient because synchronization of
  // |churn_samples_| is guarded by |mutex_|.
  std::atomic<int> running_sessions_{0};
};

}  // namespace base

#endif  // BASE_SAMPLING_HEAP_PROFILER_SAMPLING_HEAP_CHURN_PROFILER_H_
