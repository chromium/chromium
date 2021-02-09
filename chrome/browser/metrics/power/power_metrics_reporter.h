// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_
#define CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/power/battery_level_provider.h"
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
  PowerMetricsReporter(
      const base::WeakPtr<UsageScenarioDataStore>& data_store,
      std::unique_ptr<BatteryLevelProvider> battery_level_provider);
  PowerMetricsReporter(const PowerMetricsReporter& rhs) = delete;
  PowerMetricsReporter& operator=(const PowerMetricsReporter& rhs) = delete;
  ~PowerMetricsReporter() override;

  BatteryLevelProvider::BatteryState& battery_state_for_testing() {
    return battery_state_;
  }

 protected:
  static const int64_t kPluggedInDischargeRateValue = -1;
  static const int64_t kBatteryStateChangedValue = -2;
  static const int64_t kInvalidDischargeRateValue = -3;
  static const int64_t kNoBatteryValue = -4;

 private:
  // performance_monitor::ProcessMonitor::Observer:
  void OnAggregatedMetricsSampled(
      const performance_monitor::ProcessMonitor::Metrics& metrics) override;

  // Report the UKMs for the past interval.
  void ReportUKMs(const performance_monitor::ProcessMonitor::Metrics& metrics,
                  base::TimeDelta interval_duration,
                  int64_t discharge_rate_during_interval) const;

  // Computes the battery discharge rate during the interval and reset
  // |battery_state_| to the current state.
  int64_t GetBatteryDischargeRataDuringInterval(
      base::TimeDelta interval_duration);

  // The data store used to get the usage scenario data, it needs to outlive
  // this class.
  base::WeakPtr<UsageScenarioDataStore> data_store_;

  std::unique_ptr<BatteryLevelProvider> battery_level_provider_;

  BatteryLevelProvider::BatteryState battery_state_{0, 0, base::nullopt, false,
                                                    base::TimeTicks::Now()};

  // The first interval will start when this class gets created.
  base::TimeTicks interval_begin_ = base::TimeTicks::Now();

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_
