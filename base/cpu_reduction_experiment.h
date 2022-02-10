// Copyright 2022 The Chromium Authors. All rights reserved.
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

// This is a helper class to reduce common duplicate code. If the CPU reduction
// experiment is running, then ShouldLogHistograms returns true on every 1000th
// call. Otherwise it always returns true.
class BASE_EXPORT CpuReductionExperimentFilter {
 public:
  // Returns true on the first call, and every 1000th call after that.
  bool ShouldLogHistograms();

 private:
  int counter_ = 0;
};

}  // namespace base

#endif  // BASE_CPU_REDUCTION_EXPERIMENT_H_
