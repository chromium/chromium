// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/sampling_heap_churn_profiler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/rand_util.h"

namespace base {

SamplingHeapChurnProfiler::SamplingHeapChurnProfiler() = default;
SamplingHeapChurnProfiler::~SamplingHeapChurnProfiler() = default;

void SamplingHeapChurnProfiler::Start() {
  // Relaxed memory ordering is sufficient since synchronization of data is
  // handled under mutex_.
  running_sessions_.fetch_add(1, std::memory_order_relaxed);
}

void SamplingHeapChurnProfiler::Stop() {
  // `fetch_sub` returns previous value.
  CHECK_GT(running_sessions_.fetch_sub(1, std::memory_order_relaxed), 0);
}

void SamplingHeapChurnProfiler::SetSubsamplingChance(double chance) {
  subsampling_chance_ = chance;
}

bool SamplingHeapChurnProfiler::ShouldRecordAllocFree() const {
  if (running_sessions_.load(std::memory_order_relaxed) == 0) {
    return false;
  }
  return base::RandDouble() < subsampling_chance_;
}

std::vector<SamplingHeapProfiler::Sample>
SamplingHeapChurnProfiler::TakeSamples() {
  AutoLock lock(mutex_);
  std::vector<SamplingHeapProfiler::Sample> samples = std::move(churn_samples_);
  churn_samples_.clear();
  return samples;
}

void SamplingHeapChurnProfiler::RecordAllocFree(
    SamplingHeapProfiler::Sample sample) {
  AutoLock lock(mutex_);
  CHECK_GT(subsampling_chance_, 0.0);
  // Scale total size by inverse subsampling chance to estimate total byte
  // churn.
  sample.total /= subsampling_chance_;
  churn_samples_.push_back(std::move(sample));
}

}  // namespace base
