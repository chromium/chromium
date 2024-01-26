// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_WIN_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_WIN_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/performance_monitor/system_monitor.h"

namespace performance_monitor {

class MetricEvaluatorsHelperWin : public MetricEvaluatorsHelper {
 public:
  MetricEvaluatorsHelperWin(const MetricEvaluatorsHelperWin&) = delete;
  MetricEvaluatorsHelperWin& operator=(const MetricEvaluatorsHelperWin&) =
      delete;

  ~MetricEvaluatorsHelperWin() override;

  // MetricEvaluatorsHelper:
  std::optional<int> GetFreePhysicalMemoryMb() override;

 private:
  friend class MetricEvaluatorsHelperWinTest;
  friend class SystemMonitor;

  // The constructor is made private to enforce that there's only one instance
  // of this class existing at the same time. In practice this instance is meant
  // to be instantiated by the SystemMonitor global instance.
  MetricEvaluatorsHelperWin();

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_WIN_H_
