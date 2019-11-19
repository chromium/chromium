// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_WIN_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_WIN_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "chrome/browser/performance_monitor/system_monitor.h"
#include "chrome/browser/performance_monitor/wmi_refresher.h"

namespace performance_monitor {

class MetricEvaluatorsHelperWin : public MetricEvaluatorsHelper {
 public:
  ~MetricEvaluatorsHelperWin() override;

  // MetricEvaluatorsHelper:
  base::Optional<int> GetFreePhysicalMemoryMb() override;
  base::Optional<float> GetDiskIdleTimePercent() override;
  base::Optional<int> GetChromeTotalResidentSetEstimateMb() override;

  bool wmi_refresher_initialized_for_testing() {
    return wmi_refresher_initialized_;
  }

 private:
  friend class MetricEvaluatorsHelperWinTest;
  friend class SystemMonitor;

  // The constructor is made private to enforce that there's only one instance
  // of this class existing at the same time. In practice this instance is meant
  // to be instantiated by the SystemMonitor global instance.
  MetricEvaluatorsHelperWin();

  // Callback that should be called once the initialization of the WMI refresher
  // has completed.
  void OnWMIRefresherInitialized(bool init_success) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    wmi_refresher_initialized_ = init_success;
  }

  // Indicates if the WMI refresher has been initialized.
  bool wmi_refresher_initialized_ = false;

  // The sequence on which the WMI refresher is going to be initialized.
  scoped_refptr<base::SequencedTaskRunner> wmi_initialization_sequence_;

  // The WMI refresher used to retrieve performance data via WMI.
  const std::unique_ptr<win::WMIRefresher, base::OnTaskRunnerDeleter>
      wmi_refresher_;

  // The number of consecutive WMI failures.
  size_t wmi_consecutive_failure_count_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MetricEvaluatorsHelperWin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MetricEvaluatorsHelperWin);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_WIN_H_
