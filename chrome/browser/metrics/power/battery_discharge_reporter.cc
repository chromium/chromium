// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/battery_discharge_reporter.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/metrics/power/power_metrics.h"
#include "chrome/browser/metrics/power/process_metrics_recorder_util.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario.h"

#if BUILDFLAG(IS_MAC)
#include "base/metrics/histogram_functions.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

bool IsWithinTolerance(base::TimeDelta value,
                       base::TimeDelta expected,
                       base::TimeDelta tolerance) {
  return (value - expected).magnitude() < tolerance;
}

}  // namespace

BatteryDischargeReporter::BatteryDischargeReporter(
    base::BatteryStateSampler* battery_state_sampler,
    UsageScenarioDataStore* battery_usage_scenario_data_store)
    : battery_usage_scenario_data_store_(battery_usage_scenario_data_store) {
  if (!battery_usage_scenario_data_store_) {
    battery_usage_scenario_tracker_ = std::make_unique<UsageScenarioTracker>();
    battery_usage_scenario_data_store_ =
        battery_usage_scenario_tracker_->data_store();
  }

  scoped_battery_state_sampler_observation_.Observe(battery_state_sampler);
}

BatteryDischargeReporter::~BatteryDischargeReporter() = default;

void BatteryDischargeReporter::OnBatteryStateSampled(
    const absl::optional<base::BatteryLevelProvider::BatteryState>&
        battery_state) {
  base::TimeTicks now_ticks = base::TimeTicks::Now();

  // First sampling event. Remember the time and skip.
  if (!last_event_time_ticks_) {
    last_event_time_ticks_ = now_ticks;
    last_battery_state_ = battery_state;
    return;
  }

  base::TimeDelta sampling_event_delta = now_ticks - *last_event_time_ticks_;
  *last_event_time_ticks_ = now_ticks;

#if BUILDFLAG(IS_MAC)
  RecordIOPMPowerSourceSampleEventDelta(sampling_event_delta);
#endif

  // Evaluate battery discharge mode and rate.
  auto battery_discharge = GetBatteryDischargeDuringInterval(
      last_battery_state_, battery_state, sampling_event_delta);
  last_battery_state_ = battery_state;

  // Intervals are expected to be approximately 1 minute long. Exclude samples
  // where the interval length deviate significantly from that value. 1 second
  // tolerance was chosen to include ~70% of all samples.
  if (battery_discharge.mode == BatteryDischargeMode::kDischarging &&
      !IsWithinTolerance(sampling_event_delta, base::Minutes(1),
                         base::Seconds(1))) {
    battery_discharge.mode = BatteryDischargeMode::kInvalidInterval;
  }

#if BUILDFLAG(IS_WIN)
  if (battery_discharge.mode == BatteryDischargeMode::kDischarging) {
    base::UmaHistogramBoolean(
        "Power.BatteryDischargeGranularityAvailable",
        battery_state->battery_discharge_granularity.has_value());

    if (battery_state->battery_discharge_granularity.has_value()) {
      base::UmaHistogramCustomCounts(
          "Power.BatteryDischargeGranularityMilliwattHours2",
          battery_state->battery_discharge_granularity.value(),
          /*min=*/0, /*exclusive_max=*/20000,
          /*buckets=*/50);

      uint32_t granularity_relative =
          battery_state->battery_discharge_granularity.value() * 10000 /
          battery_state->full_charged_capacity.value();
      base::UmaHistogramCustomCounts(
          "Power.BatteryDischargeGranularityRelative2", granularity_relative,
          /*min=*/0, /*exclusive_max=*/20000,
          /*buckets=*/50);
    }
  }
#endif

  auto interval_data = battery_usage_scenario_data_store_->ResetIntervalData();

  // Get scenario data.
  const auto long_interval_scenario_params =
      GetLongIntervalScenario(interval_data);
  // Histograms are recorded without suffix and with a scenario-specific
  // suffix.
  const std::vector<const char*> long_interval_suffixes{
      "", long_interval_scenario_params.histogram_suffix};
  ReportBatteryHistograms(sampling_event_delta, battery_discharge,
                          is_initial_interval_, long_interval_suffixes);
  is_initial_interval_ = false;
}

#if BUILDFLAG(IS_MAC)
void BatteryDischargeReporter::RecordIOPMPowerSourceSampleEventDelta(
    base::TimeDelta sampling_event_delta) {
  // The delta is expected to be almost always 60 seconds. Split the buckets for
  // 0.2s granularity (10s interval with 50 buckets + 1 underflow bucket + 1
  // overflow bucket) around that value.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryTimeGet(
      "Power.IOPMPowerSource.SamplingEventDelta",
      /*min=*/base::Seconds(55), /*max=*/base::Seconds(65), /*buckets=*/52,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(sampling_event_delta);

  // Same as the above but using a range that starts from zero and significantly
  // goes beyond the expected mean time of |sampling_event_delta| (which is 60
  // seconds.).
  base::UmaHistogramMediumTimes(
      "Power.IOPMPowerSource.SamplingEventDelta.MediumTimes",
      sampling_event_delta);
}
#endif  // BUILDFLAG(IS_MAC)
