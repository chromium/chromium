// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cpu_reduction_experiment.h"

#include <atomic>

#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/rand_util.h"

namespace base {

namespace {

// Whether to enable a series of optimizations that reduce total CPU
// utilization.
BASE_FEATURE(kReduceCpuUtilization,
             "ReduceCpuUtilization2",
             FEATURE_ENABLED_BY_DEFAULT);

class CpuReductionExperimentSubSampler {
 public:
  CpuReductionExperimentSubSampler() : counter_(base::RandUint64()) {}

  bool ShouldLogHistograms() {
    // Relaxed memory order since there is no dependent memory access.
    uint64_t val = counter_.fetch_add(1, std::memory_order_relaxed);
    return val % 1000 == 0;
  }

 private:
  std::atomic<uint64_t> counter_{0};
};

// Singleton instance of CpuReductionExperimentSubSampler. This is only set when
// the ReduceCpuUtilization experiment is enabled -- as a result, it's ok to
// assume that the experiment is disabled when this is not set.
CpuReductionExperimentSubSampler* g_subsampler = nullptr;

#if DCHECK_IS_ON()
// Atomic to support concurrent writes from IsRunningCpuReductionExperiment().
std::atomic_bool g_accessed_subsampler = false;
#endif

}  // namespace

bool IsRunningCpuReductionExperiment() {
#if DCHECK_IS_ON()
  // Relaxed memory order since there is no dependent memory access.
  g_accessed_subsampler.store(true, std::memory_order_relaxed);
#endif
  return !!g_subsampler;
}

void InitializeCpuReductionExperiment() {
#if DCHECK_IS_ON()
  // TSAN should generate an error if InitializeCpuReductionExperiment() races
  // with IsRunningCpuReductionExperiment().
  //
  // Relaxed memory order since there is no dependent memory access.
  DCHECK(!g_accessed_subsampler.load(std::memory_order_relaxed));
#endif
  if (FeatureList::IsEnabled(kReduceCpuUtilization)) {
    g_subsampler = new CpuReductionExperimentSubSampler();
  }
}

bool ShouldLogHistogramForCpuReductionExperiment() {
  if (!IsRunningCpuReductionExperiment())
    return true;
  return g_subsampler->ShouldLogHistograms();
}

}  // namespace base
