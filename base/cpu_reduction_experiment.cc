// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cpu_reduction_experiment.h"

#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"

namespace base {

namespace {

// This feature controls whether to enable a series of optimizations that
// reduces total CPU utilization of chrome.
constexpr Feature kReduceCpuUtilization{"ReduceCpuUtilization",
                                        FEATURE_DISABLED_BY_DEFAULT};

// Cache of the state of the ReduceCpuUtilization feature. This avoids the need
// to constantly query its enabled state through FeatureList::IsEnabled().
bool g_is_reduce_cpu_enabled =
    kReduceCpuUtilization.default_state == FEATURE_ENABLED_BY_DEFAULT;

#if DCHECK_IS_ON()
// Atomic to support concurrent writes from IsRunningCpuReductionExperiment().
std::atomic_bool g_accessed_is_reduce_cpu_enabled = false;
#endif

}  // namespace

bool IsRunningCpuReductionExperiment() {
#if DCHECK_IS_ON()
  g_accessed_is_reduce_cpu_enabled.store(true, std::memory_order_seq_cst);
#endif
  return g_is_reduce_cpu_enabled;
}

void InitializeCpuReductionExperiment() {
#if DCHECK_IS_ON()
  // TSAN should generate an error if InitializeCpuReductionExperiment() races
  // with IsRunningCpuReductionExperiment().
  DCHECK(!g_accessed_is_reduce_cpu_enabled.load(std::memory_order_seq_cst));
#endif
  g_is_reduce_cpu_enabled = FeatureList::IsEnabled(kReduceCpuUtilization);
}

bool CpuReductionExperimentFilter::ShouldLogHistograms() {
  if (!IsRunningCpuReductionExperiment())
    return true;

  return counter_.fetch_add(1, std::memory_order_relaxed) % 1000 == 1;
}

}  // namespace base
