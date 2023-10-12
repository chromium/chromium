// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cpu_reduction_experiment.h"

#include <atomic>

#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace base {

namespace {

// Whether to enable a series of optimizations that reduce total CPU
// utilization.
BASE_FEATURE(kReduceCpuUtilization,
             "ReduceCpuUtilization2",
             FEATURE_ENABLED_BY_DEFAULT);

class CpuReductionExperimentSubSampler {
 public:
  CpuReductionExperimentSubSampler() = default;

  bool ShouldLogHistograms() {
    AutoLock hold(lock_);
    return sub_sampler_.ShouldSample(0.001);
  }

 private:
  Lock lock_;
  MetricsSubSampler sub_sampler_ GUARDED_BY(lock_);
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
  g_accessed_subsampler.store(true, std::memory_order_seq_cst);
#endif
  return !!g_subsampler;
}

void InitializeCpuReductionExperiment() {
#if DCHECK_IS_ON()
  // TSAN should generate an error if InitializeCpuReductionExperiment() races
  // with IsRunningCpuReductionExperiment().
  DCHECK(!g_accessed_subsampler.load(std::memory_order_seq_cst));
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
