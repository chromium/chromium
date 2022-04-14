// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_
#define CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/power/battery_level_provider.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_tracker.h"
#include "chrome/browser/performance_monitor/process_monitor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/metrics/power/coalition_resource_usage_provider_mac.h"
#include "components/power_metrics/iopm_power_source_sampling_event_source.h"
#endif  // BUILDFLAG(IS_MAC)

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
// This class should be created shortly after
// performance_monitor::ProcessMonitor.
//
// Previous histograms reported by this class:
// - Main screen brightness, removed by https://crrev.com/c/3431905
// Historical data: go/chrome-historical-power-histograms
class PowerMetricsReporter
    : public performance_monitor::ProcessMonitor::Observer {
 public:
  using ProcessMonitor = performance_monitor::ProcessMonitor;
#if BUILDFLAG(IS_MAC)
  using CoalitionResourceUsageRate = power_metrics::CoalitionResourceUsageRate;
#endif  // BUILDFLAG(IS_MAC)

  static constexpr base::TimeDelta kShortIntervalDuration = base::Seconds(10);

  // Used to calculate the duration of a short interval. Set from at the
  // beginning of a short interval (OnShortIntervalBegin()) and reset at the end
  // (ReportShortIntervalHistograms()).
  base::TimeTicks short_interval_begin_time_;

  // Use the default arguments in production. In tests, use arguments to provide
  // mocks. |(short|long)_usage_scenario_data_store| are queried to determine
  // the scenario for short and long reporting intervals. They must outlive the
  // PowerMetricsReporter. |battery_level_provider| is used to obtain the
  // battery level.
  explicit PowerMetricsReporter(
      UsageScenarioDataStore* short_usage_scenario_data_store = nullptr,
      UsageScenarioDataStore* long_usage_scenario_data_store = nullptr,
      std::unique_ptr<BatteryLevelProvider> battery_level_provider =
          BatteryLevelProvider::Create()
#if BUILDFLAG(IS_MAC)
          ,
      std::unique_ptr<CoalitionResourceUsageProvider>
          coalition_resource_usage_provider =
              std::make_unique<CoalitionResourceUsageProvider>()
#endif  // BUILDFLAG(IS_MAC)
  );
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
  static std::vector<const char*> GetLongIntervalSuffixesForTesting(
      const UsageScenarioDataStore::IntervalData& interval_data);

  // Contains data to determine when and how to generate histograms and trace
  // events for a usage scenario.
  struct ScenarioParams {
    const char* histogram_suffix;
    // CPU usage threshold to emit a "high CPU" trace event.
    double short_interval_cpu_threshold;
    const char* trace_event_title;
  };

#if BUILDFLAG(IS_MAC)
  // Returns params to use for histograms and trace events related to a short
  // interval described by `short_interval_data`. `pre_interval_data` describes
  // a long interval ending simultaneously with the short interval.
  //
  // `pre_interval_data` is required to decide whether "_Recent" is appended to
  // the ".ZeroWindow" or ".AllTabsHidden_NoVideoCaptureOrAudio" suffixes.
  // Appending "_Recent" is useful  to isolate cases where the scenario changed
  // recently (e.g. CPU usage in a short interval with zero window might be
  // affected by cleanup tasks from recently closed tabs).
  static const PowerMetricsReporter::ScenarioParams&
  GetShortIntervalScenarioParams(
      const UsageScenarioDataStore::IntervalData& short_interval_data,
      const UsageScenarioDataStore::IntervalData& pre_interval_data);
#endif  // BUILDFLAG(IS_MAC)

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

  struct BatteryDischarge {
    PowerMetricsReporter::BatteryDischargeMode mode;
    // Discharge rate in 1/10000 of full capacity per minute.
    absl::optional<int64_t> rate;
  };

  static void ReportLongIntervalHistograms(
      const UsageScenarioDataStore::IntervalData& interval_data,
      const ProcessMonitor::Metrics& aggregated_process_metrics,
      base::TimeDelta interval_duration,
      BatteryDischarge battery_discharge
#if BUILDFLAG(IS_MAC)
      ,
      const absl::optional<CoalitionResourceUsageRate>&
          coalition_resource_usage_rate
#endif
  );

#if BUILDFLAG(IS_MAC)
  static void ReportShortIntervalHistograms(
      const char* scenario_suffix,
      absl::optional<CoalitionResourceUsageRate> coalition_resource_usage_rate);

  // Emit trace event when CPU usage is high for 10 secondes or more.
  void MaybeEmitHighCPUTraceEvent(
      const ScenarioParams& short_interval_scenario_params,
      absl::optional<CoalitionResourceUsageRate> coalition_resource_usage_rate);
#endif  // BUILDFLAG(IS_MAC)

  // Report battery metrics to histograms with |suffixes|.
  static void ReportBatteryHistograms(base::TimeDelta interval_duration,
                                      BatteryDischarge battery_discharge,
                                      const std::vector<const char*>& suffixes);

  // Report aggregated process metrics to histograms with |suffixes|.
  static void ReportAggregatedProcessMetricsHistograms(
      const ProcessMonitor::Metrics& aggregated_process_metrics,
      const std::vector<const char*>& suffixes);

#if BUILDFLAG(IS_MAC)
  // Report resource coalition metrics to histograms with |suffixes|.
  static void ReportResourceCoalitionHistograms(
      const power_metrics::CoalitionResourceUsageRate& rate,
      const std::vector<const char*>& suffixes);
#endif  // BUILDFLAG(IS_MAC)

 private:
  // ProcessMonitor::Observer:
  void OnAggregatedMetricsSampled(
      const ProcessMonitor::Metrics& aggregated_process_metrics) override;

  void OnFirstBatteryStateSampled(
      const BatteryLevelProvider::BatteryState& battery_state);
  void OnBatteryAndAggregatedProcessMetricsSampled(
      const ProcessMonitor::Metrics& aggregated_process_metrics,
      base::TimeTicks battery_sample_begin_time,
      const BatteryLevelProvider::BatteryState& battery_state);

  // Report the UKMs for the past interval.
  void ReportUKMs(const UsageScenarioDataStore::IntervalData& interval_data,
                  const ProcessMonitor::Metrics& aggregated_process_metrics,
                  base::TimeDelta interval_duration,
                  BatteryDischarge battery_discharge) const;

  // Computes and returns the battery discharge mode and rate during the
  // interval, and reset |battery_state_| to the current state. If the discharge
  // rate isn't valid, the returned value is nullopt and the reason is indicated
  // per BatteryDischargeMode.
  BatteryDischarge GetBatteryDischargeDuringInterval(
      const BatteryLevelProvider::BatteryState& new_battery_state,
      base::TimeDelta interval_duration);

#if BUILDFLAG(IS_MAC)
  // Invoked at the beginning of a "short" interval (~10 seconds before
  // OnAggregatedMetricsSampled).
  void OnShortIntervalBegin();

  void OnIOPMPowerSourceSamplingEvent();
#endif  // BUILDFLAG(IS_MAC)

  // Track usage scenarios over 10-seconds and 2-minutes intervals. In
  // production, the data stores are obtained from the trackers, but in tests
  // they may be mocks injected via the constructor.
  std::unique_ptr<UsageScenarioTracker> short_usage_scenario_tracker_;
  raw_ptr<UsageScenarioDataStore> short_usage_scenario_data_store_;
  std::unique_ptr<UsageScenarioTracker> long_usage_scenario_tracker_;
  raw_ptr<UsageScenarioDataStore> long_usage_scenario_data_store_;

  std::unique_ptr<BatteryLevelProvider> battery_level_provider_;

  BatteryLevelProvider::BatteryState battery_state_{0, 0, absl::nullopt, false,
                                                    base::TimeTicks::Now()};

  base::TimeTicks interval_begin_;

  base::OnceClosure on_battery_sampled_for_testing_;

#if BUILDFLAG(IS_MAC)
  // Timer that fires at the beginning of a "short" interval. This is 10 seconds
  // before a call to OnAggregatedMetricsSampled() is expected.
  base::RetainingOneShotTimer short_interval_timer_;

  power_metrics::IOPMPowerSourceSamplingEventSource
      iopm_power_source_sampling_event_source_;

  // The time ticks from when the last IOPMPowerSource event was received.
  absl::optional<base::TimeTicks> last_event_time_ticks_;

  std::unique_ptr<CoalitionResourceUsageProvider>
      coalition_resource_usage_provider_;
#endif  // BUILDFLAG(IS_MAC)

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_METRICS_POWER_POWER_METRICS_REPORTER_H_
