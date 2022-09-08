// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CPU_REDUCTION_EXPERIMENT_H_
#define BASE_CPU_REDUCTION_EXPERIMENT_H_

#include "base/base_export.h"

namespace base {

// Returns whether the cpu cycle reduction experiment is running.
// The goal of this experiment is to better understand the relationship between
// total CPU cycles used across the fleet and top-line chrome metrics.
BASE_EXPORT bool IsRunningCpuReductionExperiment();

// Must be called after FeatureList initialization and while chrome is still
// single-threaded.
BASE_EXPORT void InitializeCpuReductionExperiment();

// Returns true if the next sample should be recorded to an histogram
// sub-sampled under the CPU reduction experiment. Returns true randomly for
// ~1/1000 calls when the experiment is enabled, or always returns true when the
// experiment is disabled.
BASE_EXPORT bool ShouldLogHistogramForCpuReductionExperiment();

}  // namespace base

#endif  // BASE_CPU_REDUCTION_EXPERIMENT_H_
