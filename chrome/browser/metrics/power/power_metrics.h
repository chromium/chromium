// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_POWER_METRICS_H_
#define CHROME_BROWSER_METRICS_POWER_POWER_METRICS_H_

#include <vector>

#include "base/power_monitor/power_monitor_buildflags.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/power/process_monitor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/metrics/power/coalition_resource_usage_provider_mac.h"
#include "components/power_metrics/resource_coalition_mac.h"
#endif

#if BUILDFLAG(HAS_BATTERY_LEVEL_PROVIDER_IMPL)
#include "base/power_monitor/battery_level_provider.h"
#endif

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
  // Discharge rate in 1/10000 of full capacity per minute.
  absl::optional<int64_t> rate;
};

#if BUILDFLAG(HAS_BATTERY_LEVEL_PROVIDER_IMPL)
// Computes and returns the battery discharge mode and rate during the interval.
// If the discharge rate isn't valid, the returned rate is nullopt and the
// reason is indicated per BatteryDischargeMode.
BatteryDischarge GetBatteryDischargeDuringInterval(
    const absl::optional<base::BatteryLevelProvider::BatteryState>&
        previous_battery_state,
    const absl::optional<base::BatteryLevelProvider::BatteryState>&
        new_battery_state,
    base::TimeDelta interval_duration);

// Report battery metrics to histograms with |suffixes|.
void ReportBatteryHistograms(base::TimeDelta interval_duration,
                             BatteryDischarge battery_discharge,
                             const std::vector<const char*>& suffixes);
#endif  // BUILDFLAG(HAS_BATTERY_LEVEL_PROVIDER_IMPL)

#if BUILDFLAG(IS_MAC)
void ReportShortIntervalHistograms(
    const char* scenario_suffix,
    const power_metrics::CoalitionResourceUsageRate&
        coalition_resource_usage_rate);

// Report resource coalition metrics to histograms with |suffixes|.
void ReportResourceCoalitionHistograms(
    const power_metrics::CoalitionResourceUsageRate& rate,
    const std::vector<const char*>& suffixes);
#endif  // BUILDFLAG(IS_MAC)

#endif  // CHROME_BROWSER_METRICS_POWER_POWER_METRICS_H_
