// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_POSIX_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_POSIX_H_

#include "chrome/browser/performance_monitor/system_monitor.h"

namespace performance_monitor {

class MetricEvaluatorsHelperPosix : public MetricEvaluatorsHelper {
 public:
  MetricEvaluatorsHelperPosix();

  MetricEvaluatorsHelperPosix(const MetricEvaluatorsHelperPosix&) = delete;
  MetricEvaluatorsHelperPosix& operator=(const MetricEvaluatorsHelperPosix&) =
      delete;

  ~MetricEvaluatorsHelperPosix() override;

  // MetricEvaluatorsHelper:
  std::optional<int> GetFreePhysicalMemoryMb() override;
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_POSIX_H_
