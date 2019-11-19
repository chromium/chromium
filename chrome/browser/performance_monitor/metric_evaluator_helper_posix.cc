// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric_evaluator_helper_posix.h"

#include "base/logging.h"

namespace performance_monitor {

MetricEvaluatorsHelperPosix::MetricEvaluatorsHelperPosix() = default;
MetricEvaluatorsHelperPosix::~MetricEvaluatorsHelperPosix() = default;

base::Optional<int> MetricEvaluatorsHelperPosix::GetFreePhysicalMemoryMb() {
  NOTREACHED();
  return base::nullopt;
}

base::Optional<float> MetricEvaluatorsHelperPosix::GetDiskIdleTimePercent() {
  NOTREACHED();
  return base::nullopt;
}

base::Optional<int>
MetricEvaluatorsHelperPosix::GetChromeTotalResidentSetEstimateMb() {
  NOTREACHED();
  return base::nullopt;
}

}  // namespace performance_monitor
