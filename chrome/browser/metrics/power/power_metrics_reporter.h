// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_
#define CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/performance_monitor/process_monitor.h"

// Reports metrics related to power (battery discharge, cpu time, etc.) to
// understand what impacts Chrome's power consumption over an interval of time.
//
// This class and its associated data store should be created shortly after
// performance_monitor::ProcessMonitor.
class PowerMetricsReporter
    : public performance_monitor::ProcessMonitor::Observer {
 public:
  // |data_store| will be queried at regular interval to report the metrics, it
  // needs to outlive this class.
  explicit PowerMetricsReporter(
      const base::WeakPtr<UsageScenarioDataStore>& data_store);
  PowerMetricsReporter(const PowerMetricsReporter& rhs) = delete;
  PowerMetricsReporter& operator=(const PowerMetricsReporter& rhs) = delete;
  ~PowerMetricsReporter() override;

 private:
  // performance_monitor::ProcessMonitor::Observer:
  void OnAggregatedMetricsSampled(
      const performance_monitor::ProcessMonitor::Metrics& metrics) override;

  // Report the UKMs for the past interval.
  void ReportUKMs(const performance_monitor::ProcessMonitor::Metrics& metrics,
                  base::TimeDelta interval_duration) const;

  // The data store used to get the usage scenario data, it needs to outlive
  // this class.
  base::WeakPtr<UsageScenarioDataStore> data_store_;

  // The first interval will start when this class gets created.
  base::TimeTicks interval_begin_ = base::TimeTicks::Now();

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_
