// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_POWER_METRICS_H_
#define CHROME_BROWSER_METRICS_POWER_POWER_METRICS_H_

#include <optional>
#include <vector>

#include "base/power_monitor/battery_level_provider.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/power/process_monitor.h"

// Report aggregated process metrics to histograms with |suffixes|.
void ReportAggregatedProcessMetricsHistograms(
    const ProcessMonitor::Metrics& aggregated_process_metrics,
    const std::vector<const char*>& suffixes);

// Any change to this enum should be reflected in the corresponding enums.xml
// and ukm.xml
enum class BatteryDischargeMode {
  kDischarging = 0,
  kPluggedIn = 1,
  kStateChanged = 2,
  kRetrievalError = 3,
  kNoBattery = 4,
  kBatteryLevelIncreased = 5,
  kInvalidInterval = 6,
  kMacFullyCharged = 7,
  kMultipleBatteries = 8,
  kFullChargedCapacityIsZero = 9,
  kInsufficientResolution = 10,
  kMaxValue = kInsufficientResolution,
};

struct BatteryDischarge {
  BatteryDischargeMode mode;
  // Discharge rate in milliwatts.
  std::optional<int64_t> rate_milliwatts;
#if BUILDFLAG(IS_WIN)
  // Discharge rate in milliwatts, if the client's battery discharge granularity
  // is at most 17 mWh. Calculated using the used capacity instead of the
  // current capacity.
  std::optional<int64_t> rate_milliwatts_with_precise_granularity;
#endif  // BUILDFLAG(IS_WIN)
  // Discharge rate in hundredth of a percent per minute.
  std::optional<int64_t> rate_relative;
};

// Returns the discharge rate in milliwatts.
int64_t CalculateDischargeRateMilliwatts(
    const base::BatteryLevelProvider::BatteryState& previous_battery_state,
    const base::BatteryLevelProvider::BatteryState& new_battery_state,
    base::TimeDelta interval_duration);

// Returns the discharge rate in one hundredth of a percent of full capacity per
// minute.
int64_t CalculateDischargeRateRelative(
    const base::BatteryLevelProvider::BatteryState& previous_battery_state,
    const base::BatteryLevelProvider::BatteryState& new_battery_state,
    base::TimeDelta interval_duration);

// Computes and returns the battery discharge mode and rate during the interval.
// If the discharge rate isn't valid, the returned rate is nullopt and the
// reason is indicated per BatteryDischargeMode.
BatteryDischarge GetBatteryDischargeDuringInterval(
    const std::optional<base::BatteryLevelProvider::BatteryState>&
        previous_battery_state,
    const std::optional<base::BatteryLevelProvider::BatteryState>&
        new_battery_state,
    base::TimeDelta interval_duration);

// Report battery metrics to histograms with |scenario_suffixes|.
void ReportBatteryHistograms(base::TimeDelta interval_duration,
                             BatteryDischarge battery_discharge,
                             bool is_initial_interval,
                             const std::vector<const char*>& scenario_suffixes);

#if BUILDFLAG(IS_WIN)
// Report battery metrics captured over a >10 minutes interval.
void ReportBatteryHistogramsTenMinutesInterval(
    base::TimeDelta interval_duration,
    BatteryDischarge battery_discharge);
#endif  // BUILDFLAG(IS_WIN)

#endif  // CHROME_BROWSER_METRICS_POWER_POWER_METRICS_H_
