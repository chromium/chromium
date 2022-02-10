// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cpu_reduction_experiment.h"

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

}  // namespace

bool IsRunningCpuReductionExperiment() {
  return g_is_reduce_cpu_enabled;
}

void InitializeCpuReductionExperiment() {
  g_is_reduce_cpu_enabled = FeatureList::IsEnabled(kReduceCpuUtilization);
}

bool CpuReductionExperimentFilter::ShouldLogHistograms() {
  if (!IsRunningCpuReductionExperiment())
    return true;

  return (++counter_ % 1000) == 1;
}

}  // namespace base
