// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_BATTERY_DISCHARGE_REPORTER_H_
#define CHROME_BROWSER_METRICS_POWER_BATTERY_DISCHARGE_REPORTER_H_

#include <memory>
#include <optional>

#include "base/power_monitor/battery_state_sampler.h"
#include "base/power_monitor/power_monitor_buildflags.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_tracker.h"

class BatteryDischargeReporter : public base::BatteryStateSampler::Observer {
 public:
  explicit BatteryDischargeReporter(
      base::BatteryStateSampler* battery_state_sampler,
      UsageScenarioDataStore* battery_usage_scenario_data_store = nullptr);

  BatteryDischargeReporter(const BatteryDischargeReporter&) = delete;
  BatteryDischargeReporter& operator=(const BatteryDischargeReporter&) = delete;
  BatteryDischargeReporter(BatteryDischargeReporter&&) = delete;
  BatteryDischargeReporter& operator=(BatteryDischargeReporter&&) = delete;

  ~BatteryDischargeReporter() override;

  // base::BatteryStateSampler::Observer:
  void OnBatteryStateSampled(
      const std::optional<base::BatteryLevelProvider::BatteryState>&
          battery_state) override;

 private:
  base::TimeDelta GetSampleInterval() const;

  // A sample interval is considered valid if it is equal to `sample_interval_`,
  // plus or minus a second.
  bool IsValidSampleInterval(base::TimeDelta interval_duration);

  // Resets the state tracking the one minute interval.
  void StartNewOneMinuteInterval(
      const std::optional<base::BatteryLevelProvider::BatteryState>&
          battery_state);

#if BUILDFLAG(IS_WIN)
  // Resets the state tracking the ten minutes interval.
  void StartNewTenMinutesInterval(
      const std::optional<base::BatteryLevelProvider::BatteryState>&
          battery_state);
#endif  // BUILDFLAG(IS_WIN)

  // Reports battery discharge histograms for a 1 minute interval.
  void ReportOneMinuteInterval(
      base::TimeDelta interval_duration,
      const std::optional<base::BatteryLevelProvider::BatteryState>&
          battery_state);

#if BUILDFLAG(IS_WIN)
  // Reports battery discharge histograms for a 10 minutes interval.
  //
  // On Windows, the reported battery discharge over a 1 minute interval is
  // frequently zero. This could be explained by some systems having a battery
  // level granularity insufficient to measure the typical discharge over a
  // 1 minute interval or by a refresh rate lower than once per minute. We'll
  // verify if using a longer interval alleviates the problem.
  void ReportTenMinutesInterval(
      base::TimeDelta interval_duration,
      const std::optional<base::BatteryLevelProvider::BatteryState>&
          battery_state);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
  // Records the time delta between two events received from IOPMPowerSource.
  void RecordIOPMPowerSourceSampleEventDelta(
      base::TimeDelta sampling_event_delta);
#endif  // BUILDFLAG(IS_MAC)

  base::ScopedObservation<base::BatteryStateSampler,
                          base::BatteryStateSampler::Observer>
      scoped_battery_state_sampler_observation_{this};

  // The sample interval retrieved from the BatteryStateSampler.
  base::TimeDelta sample_interval_;

  // Track usage scenarios for the sampling interval.
  std::unique_ptr<UsageScenarioTracker> battery_usage_scenario_tracker_;
  raw_ptr<UsageScenarioDataStore> battery_usage_scenario_data_store_;

  // The time at the last sample received from `sampling_event_source_`.
  // Is equal to nullopt on the first sample.
  std::optional<base::TimeTicks> last_sample_time_;

  // The number of consecutive samples since the last recording of the 1min
  // battery discharge rate.
  int one_minute_sample_count_ = 0;

  // The time passed since the last recording of the 1min battery discharge
  // rate.
  base::TimeDelta one_minute_interval_duration_;

  // The battery state at the time the last recording of the 1min battery state.
  std::optional<base::BatteryLevelProvider::BatteryState>
      one_minute_interval_start_battery_state_;

#if BUILDFLAG(IS_WIN)
  // The number of consecutive samples since the last recording of the 10mins
  // battery discharge rate.
  int ten_minutes_sample_count_ = 0;

  // The time passed since the last recording of the 10mins battery discharge
  // rate.
  base::TimeDelta ten_minutes_interval_duration_;

  // The battery state at the time the last recording of the 10mins battery
  // state.
  std::optional<base::BatteryLevelProvider::BatteryState>
      ten_minutes_interval_start_battery_state_;
#endif  // BUILDFLAG(IS_WIN)

  // The first battery sample is potentially outdated because it is not taken
  // upon receiving a notification from the OS. This is used to differentiate
  // the first battery discharge histogram sample from the rest as that initial
  // one is potentially skewed.
  bool is_initial_interval_ = true;
};

#endif  // CHROME_BROWSER_METRICS_POWER_BATTERY_DISCHARGE_REPORTER_H_
