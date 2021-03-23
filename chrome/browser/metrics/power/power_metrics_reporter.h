// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_
#define CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_

#include <stdint.h>

#include <utility>

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

  // Ensures |callback| is called once the next battery state is available.
  void OnNextSampleForTesting(base::OnceClosure callback) {
    on_battery_sampled_for_testing_ = std::move(callback);
  }
  // Ensures |callback| is called once the first battery state is available.
  // |callback| is called synchronously if the first battery state is already
  // available.
  void OnFirstSampleForTesting(base::OnceClosure callback);

  static int64_t GetBucketForSampleForTesting(base::TimeDelta value);

 protected:
  // Any change to this enum should be reflected in the corresponding enums.xml
  // and ukm.xml
  enum class BatteryDischargeMode {
    kDischarging = 0,
    kPluggedIn = 1,
    kStateChanged = 2,
    kChargeLevelUnavailable = 3,
    kNoBattery = 4,
    kInvalidDischargeRate = 5,
    kInvalidInterval = 6,
    kMaxValue = kInvalidInterval
  };

  // Report the histograms for the past interval, with |sampling_interval| the
  // expected sampling interval, and |interval_duration| the actual duration
  // since the beginning of the interval.
  static void ReportBatteryHistograms(
      base::TimeDelta sampling_interval,
      base::TimeDelta interval_duration,
      BatteryDischargeMode discharge_mode,
      base::Optional<int64_t> discharge_rate_during_interval);

 private:
  // performance_monitor::ProcessMonitor::Observer:
  void OnAggregatedMetricsSampled(
      const performance_monitor::ProcessMonitor::Metrics& metrics) override;

  void OnFirstBatteryStateSampled(
      const BatteryLevelProvider::BatteryState& battery_state);
  void OnBatteryStateAndMetricsSampled(
      const performance_monitor::ProcessMonitor::Metrics& metrics,
      base::TimeTicks scheduled_time,
      const BatteryLevelProvider::BatteryState& battery_state);

  // Report the UKMs for the past interval.
  void ReportUKMs(const performance_monitor::ProcessMonitor::Metrics& metrics,
                  base::TimeDelta interval_duration,
                  BatteryDischargeMode discharge_mode,
                  base::Optional<int64_t> discharge_rate_during_interval) const;

  // Computes and returns the battery discharge mode and rate during the
  // interval, and reset |battery_state_| to the current state. If the discharge
  // rate isn't valid, the returned value is nullopt and the reason is indicated
  // per BatteryDischargeMode.
  std::pair<BatteryDischargeMode, base::Optional<int64_t>>
  GetBatteryDischargeRateDuringInterval(
      const BatteryLevelProvider::BatteryState& new_battery_state,
      base::TimeDelta interval_duration);

  // The data store used to get the usage scenario data, it needs to outlive
  // this class.
  base::WeakPtr<UsageScenarioDataStore> data_store_;

  std::unique_ptr<BatteryLevelProvider> battery_level_provider_;

  // Time that should elapse between calls to OnAggregatedMetricsSampled.
  base::TimeDelta desired_reporting_interval_;

  BatteryLevelProvider::BatteryState battery_state_{0, 0, base::nullopt, false,
                                                    base::TimeTicks::Now()};

  base::TimeTicks interval_begin_;

  base::OnceClosure on_battery_sampled_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PowerMetricsReporter> weak_factory_{this};
};

#endif  // CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_
