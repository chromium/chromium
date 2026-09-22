// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_limit_calculator.h"

#include <algorithm>
#include <cmath>

#include "base/check_op.h"
#include "base/memory_coordinator/utils.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/glic/public/features.h"

namespace glic {

BASE_FEATURE(kGlicMaxAwakeInstances, FEATURE_ENABLED_BY_DEFAULT_ALL_PLATFORMS);
constexpr base::FeatureParam<int> kGlicMaxAwakeInstancesLimit{
    &kGlicMaxAwakeInstances, "limit", 15};
constexpr base::FeatureParam<size_t>
    kGlicMaxAwakeInstancesModeratePressureLimit{&kGlicMaxAwakeInstances,
                                                "moderate_pressure_limit", 8};
constexpr base::FeatureParam<size_t>
    kGlicMaxAwakeInstancesCriticalPressureLimit{&kGlicMaxAwakeInstances,
                                                "critical_pressure_limit", 0};

namespace {

double LerpBetweenThresholds(size_t start_limit,
                             size_t end_limit,
                             int start_threshold,
                             int end_threshold,
                             int percentage) {
  const double t = static_cast<double>(percentage - start_threshold) /
                   (end_threshold - start_threshold);
  return std::lerp(start_limit, end_limit, t);
}

}  // namespace

size_t CalculateAwakeInstancesLimit(size_t default_limit,
                                    int memory_limit_percentage) {
  CHECK_GT(default_limit, 0u);
  CHECK_GE(memory_limit_percentage, 0);
  CHECK_GT(base::kModerateMemoryPressureThreshold, 0);
  CHECK_GT(base::kNoMemoryPressureThreshold,
           base::kModerateMemoryPressureThreshold);

  // Clamp scaled limits to at least 1 so that even under critical memory
  // pressure (0% budget), Glic always allows at least one instance to remain
  // awake, ensuring the feature stays functional for the active user.
  const size_t moderate_limit = std::clamp<size_t>(
      kGlicMaxAwakeInstancesModeratePressureLimit.Get(), 1, default_limit);
  const size_t critical_limit = std::clamp<size_t>(
      kGlicMaxAwakeInstancesCriticalPressureLimit.Get(), 1, moderate_limit);

  const double scaled_value = [&] {
    if (memory_limit_percentage <= base::kModerateMemoryPressureThreshold) {
      return LerpBetweenThresholds(critical_limit, moderate_limit,
                                   base::kCriticalMemoryPressureThreshold,
                                   base::kModerateMemoryPressureThreshold,
                                   memory_limit_percentage);
    }
    return LerpBetweenThresholds(
        moderate_limit, default_limit, base::kModerateMemoryPressureThreshold,
        base::kNoMemoryPressureThreshold, memory_limit_percentage);
  }();

  return base::ClampRound<size_t>(scaled_value);
}

}  // namespace glic
