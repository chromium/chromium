// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_
#define CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_

#include <memory>
#include <optional>
#include <utility>

#include "base/power_monitor/battery_level_provider.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/power/power_metrics.h"
#include "chrome/browser/metrics/power/process_monitor.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_tracker.h"
#include "components/performance_manager/public/power/battery_level_provider_creator.h"

// Reports metrics related to power (battery discharge, cpu time, etc.).
//
// Metrics are reported for 2 minutes intervals, with suffixes describing what
// the user did during the interval. These intervals are long enough to capture
// most side-effects of what the user did (e.g. a navigation increases memory
// usage which may cause a major GC after some time).
//
// A few metrics are also reported for 10 seconds intervals. They are meant to
// be used as triggers to upload traces from the field. Traces from the field
// only cover a few seconds and it is only possible to correctly analyze a trace
// if it covers the full interval associated with the trigger metric.
//
// This class should be created shortly after ProcessMonitor.
//
// Previous histograms reported by this class:
// - Main screen brightness, removed by https://crrev.com/c/3431905
// Historical data: go/chrome-historical-power-histograms
class PowerMetricsReporter : public ProcessMonitor::Observer {
 public:
  // Use the default arguments in production. In tests, use arguments to provide
  // mocks. |long_usage_scenario_data_store| are queried to determine the
  // scenario for long reporting intervals. They must outlive the
  // PowerMetricsReporter. |battery_level_provider| is used to obtain the
  // battery level.
  explicit PowerMetricsReporter(
      ProcessMonitor* process_monitor,
      UsageScenarioDataStore* long_usage_scenario_data_store = nullptr,
      std::unique_ptr<base::BatteryLevelProvider> battery_level_provider =
          performance_manager::power::CreateBatteryLevelProvider()
  );
  PowerMetricsReporter(const PowerMetricsReporter& rhs) = delete;
  PowerMetricsReporter& operator=(const PowerMetricsReporter& rhs) = delete;
  ~PowerMetricsReporter() override;

  std::optional<base::BatteryLevelProvider::BatteryState>&
  battery_state_for_testing() {
    return battery_state_;
  }

  static int64_t GetBucketForSampleForTesting(base::TimeDelta value);

 private:
  // Called when the initial battery state is obtained.
  void OnFirstBatteryStateSampled(
      const std::optional<base::BatteryLevelProvider::BatteryState>&
          battery_state);

  // Starts the timer for the long interval.
  void StartNextLongInterval();

  // Called when the long interval ended.
  void OnLongIntervalEnd();

  // ProcessMonitor::Observer:
  void OnMetricsSampled(MonitoredProcessType type,
                        const ProcessMonitor::Metrics& metrics) override;
  void OnAggregatedMetricsSampled(
      const ProcessMonitor::Metrics& aggregated_process_metrics) override;

  void OnBatteryAndAggregatedProcessMetricsSampled(
      const ProcessMonitor::Metrics& aggregated_process_metrics,
      base::TimeDelta interval_duration,
      const std::optional<base::BatteryLevelProvider::BatteryState>&
          new_battery_state);

  // Called when the long interval ends.
  void ReportMetrics(
      const UsageScenarioDataStore::IntervalData& long_interval_data,
      base::TimeDelta interval_duration,
      const ProcessMonitor::Metrics& aggregated_process_metrics);

  // Called after `ReportMetrics` on platforms where a `BatteryLevelProvider` is
  // available to record metrics specific to the battery state.
  void ReportBatterySpecificMetrics(
      const UsageScenarioDataStore::IntervalData& long_interval_data,
      base::TimeDelta interval_duration,
      const ProcessMonitor::Metrics& aggregated_process_metrics,
      BatteryDischarge battery_discharge);

  // Report the UKMs for the past interval.
  static void ReportBatteryUKMs(
      const UsageScenarioDataStore::IntervalData& interval_data,
      const ProcessMonitor::Metrics& aggregated_process_metrics,
      base::TimeDelta interval_duration,
      BatteryDischarge battery_discharge);

  raw_ptr<ProcessMonitor> process_monitor_;

  // Track usage scenarios over 2-minutes intervals. In production, the data
  // stores are obtained from the trackers, but in tests they may be mocks
  // injected via the constructor.
  std::unique_ptr<UsageScenarioTracker> long_usage_scenario_tracker_;
  raw_ptr<UsageScenarioDataStore> long_usage_scenario_data_store_;

  std::unique_ptr<base::BatteryLevelProvider> battery_level_provider_;
  std::optional<base::BatteryLevelProvider::BatteryState> battery_state_;

  base::TimeTicks interval_begin_;

  base::OneShotTimer interval_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_
