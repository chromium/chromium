// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_
#define CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_

#include <stdint.h>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/power/battery_level_provider.h"
#include "chrome/browser/metrics/power/power_details_provider.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/performance_monitor/process_monitor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_MAC)
#include "components/power_metrics/iopm_power_source_sampling_event_source.h"
#endif  // defined(OS_MAC)

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
  static std::vector<const char*> GetSuffixesForTesting(
      const UsageScenarioDataStore::IntervalData& interval_data);

  void set_power_details_provider_for_testing(
      std::unique_ptr<PowerDetailsProvider> provider) {
    power_details_provider_ = std::move(provider);
  }

 protected:
  // Any change to this enum should be reflected in the corresponding enums.xml
  // and ukm.xml
  enum class BatteryDischargeMode {
    kDischarging = 0,
    kPluggedIn = 1,
    kStateChanged = 2,
    kChargeLevelUnavailable = 3,
    kNoBattery = 4,
    kBatteryLevelIncreased = 5,
    kInvalidInterval = 6,
    kMacFullyCharged = 7,
    kMaxValue = kMacFullyCharged
  };

  // Report battery and CPU metrics to generic histograms and histograms with a
  // scenario suffix derived from |interval_data|.
  static void ReportHistograms(
      const UsageScenarioDataStore::IntervalData& interval_data,
      const performance_monitor::ProcessMonitor::Metrics& metrics,
      base::TimeDelta interval_duration,
      BatteryDischargeMode discharge_mode,
      absl::optional<int64_t> discharge_rate_during_interval);

  // Report battery metrics to histograms with |suffixes|.
  static void ReportBatteryHistograms(
      base::TimeDelta interval_duration,
      BatteryDischargeMode discharge_mode,
      absl::optional<int64_t> discharge_rate_during_interval,
      const std::vector<const char*>& suffixes);

  // Report CPU histograms to histograms with |suffixes|.
  static void ReportCPUHistograms(
      const performance_monitor::ProcessMonitor::Metrics& metrics,
      const std::vector<const char*>& suffixes);

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
  void ReportUKMs(const UsageScenarioDataStore::IntervalData& interval_data,
                  const performance_monitor::ProcessMonitor::Metrics& metrics,
                  base::TimeDelta interval_duration,
                  BatteryDischargeMode discharge_mode,
                  absl::optional<int64_t> discharge_rate_during_interval,
                  absl::optional<int64_t> main_screen_brightness) const;

  void ReportUKMsAndHistograms(
      const performance_monitor::ProcessMonitor::Metrics& metrics,
      base::TimeDelta interval_duration,
      BatteryDischargeMode discharge_mode,
      absl::optional<int64_t> discharge_rate_during_interval) const;

  // Computes and returns the battery discharge mode and rate during the
  // interval, and reset |battery_state_| to the current state. If the discharge
  // rate isn't valid, the returned value is nullopt and the reason is indicated
  // per BatteryDischargeMode.
  std::pair<BatteryDischargeMode, absl::optional<int64_t>>
  GetBatteryDischargeRateDuringInterval(
      const BatteryLevelProvider::BatteryState& new_battery_state,
      base::TimeDelta interval_duration);

#if defined(OS_MAC)
  void OnIOPMPowerSourceSamplingEvent();
#endif  // defined(OS_MAC)

  // The data store used to get the usage scenario data, it needs to outlive
  // this class.
  base::WeakPtr<UsageScenarioDataStore> data_store_;

  std::unique_ptr<BatteryLevelProvider> battery_level_provider_;

  std::unique_ptr<PowerDetailsProvider> power_details_provider_;

  // Time that should elapse between calls to OnAggregatedMetricsSampled.
  base::TimeDelta desired_reporting_interval_;

  BatteryLevelProvider::BatteryState battery_state_{0, 0, absl::nullopt, false,
                                                    base::TimeTicks::Now()};

  base::TimeTicks interval_begin_;

  base::OnceClosure on_battery_sampled_for_testing_;

#if defined(OS_MAC)
  power_metrics::IOPMPowerSourceSamplingEventSource
      iopm_power_source_sampling_event_source_;

  // The time ticks from when the last IOPMPowerSource event was received.
  absl::optional<base::TimeTicks> last_event_time_ticks_;
#endif  // defined(OS_MAC)

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PowerMetricsReporter> weak_factory_{this};
};

#endif  // CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_
