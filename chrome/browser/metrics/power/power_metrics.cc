// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/metrics/power/process_metrics_recorder_util.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
constexpr const char* kBatteryDischargeRateHistogramName =
    "Power.BatteryDischargeRate2";
constexpr const char* kBatteryDischargeModeHistogramName =
    "Power.BatteryDischargeMode";
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()

#if BUILDFLAG(IS_MAC)
// Reports `proportion` of a time used to a histogram in permyriad (1/100 %).
// `proportion` is 0.5 if half a CPU core or half total GPU time is used. It can
// be above 1.0 if more than 1 CPU core is used. CPU and GPU usage is often
// below 1% so it's useful to report with 1/10000 granularity (otherwise most
// samples end up in the same bucket).
void UsageTimeHistogram(const std::string& histogram_name,
                        double proportion,
                        int max_proportion) {
  // Multiplicator to convert `proportion` to permyriad (1/100 %).
  // For example, 1.0 * kScaleFactor = 10000 1/100 % = 100 %.
  constexpr int kScaleFactor = 100 * 100;

  base::UmaHistogramCustomCounts(
      histogram_name, std::lroundl(proportion * kScaleFactor),
      /* min=*/1, /* exclusive_max=*/max_proportion * kScaleFactor,
      /* buckets=*/50);
}

// Max proportion for CPU time histograms. This used to be 64 but was reduced to
// 2 because data shows that less than 0.2% of samples are above that.
constexpr int kMaxCPUProportion = 2;

// Max proportion for GPU time histograms. It's not possible to use more than
// 100% of total GPU time.
constexpr int kMaxGPUProportion = 1;
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

void ReportAggregatedProcessMetricsHistograms(
    const ProcessMonitor::Metrics& aggregated_process_metrics,
    const std::vector<const char*>& suffixes) {
  for (const char* suffix : suffixes) {
    std::string complete_suffix = base::StrCat({"Total", suffix});
    RecordProcessHistograms(complete_suffix.c_str(),
                            aggregated_process_metrics);
  }
}

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
BatteryDischarge GetBatteryDischargeDuringInterval(
    const BatteryLevelProvider::BatteryState& previous_battery_state,
    const BatteryLevelProvider::BatteryState& new_battery_state,
    base::TimeDelta interval_duration) {
  if (previous_battery_state.battery_count == 0 ||
      new_battery_state.battery_count == 0) {
    return {BatteryDischargeMode::kNoBattery, absl::nullopt};
  }
  if (!previous_battery_state.on_battery && !new_battery_state.on_battery) {
    return {BatteryDischargeMode::kPluggedIn, absl::nullopt};
  }
  if (previous_battery_state.on_battery != new_battery_state.on_battery) {
    return {BatteryDischargeMode::kStateChanged, absl::nullopt};
  }
  if (!previous_battery_state.charge_level.has_value() ||
      !new_battery_state.charge_level.has_value()) {
    return {BatteryDischargeMode::kChargeLevelUnavailable, absl::nullopt};
  }

  // The battery discharge rate is reported per minute with 1/10000 of full
  // charge resolution.
  static constexpr int64_t kDischargeRateFactor =
      10000 * base::Minutes(1).InSecondsF();

#if BUILDFLAG(IS_MAC)
  // On MacOS, empirical evidence has shown that right after a full charge, the
  // current capacity stays equal to the maximum capacity for several minutes,
  // despite the fact that power was definitely consumed. Reporting a zero
  // discharge rate for this duration would be misleading.
  if (previous_battery_state.charge_level.value() == 1.0)
    return {BatteryDischargeMode::kMacFullyCharged, absl::nullopt};
#endif

  auto discharge_rate = (previous_battery_state.charge_level.value() -
                         new_battery_state.charge_level.value()) *
                        kDischargeRateFactor / interval_duration.InSeconds();
  if (discharge_rate < 0)
    return {BatteryDischargeMode::kBatteryLevelIncreased, absl::nullopt};
  return {BatteryDischargeMode::kDischarging, discharge_rate};
}

void ReportBatteryHistograms(base::TimeDelta interval_duration,
                             BatteryDischarge battery_discharge,
                             const std::vector<const char*>& suffixes) {
  for (const char* suffix : suffixes) {
    base::UmaHistogramEnumeration(
        base::StrCat({kBatteryDischargeModeHistogramName, suffix}),
        battery_discharge.mode);

    if (battery_discharge.mode == BatteryDischargeMode::kDischarging) {
      DCHECK(battery_discharge.rate.has_value());
      base::UmaHistogramCounts1000(
          base::StrCat({kBatteryDischargeRateHistogramName, suffix}),
          *battery_discharge.rate);
    }
  }
}
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()

#if BUILDFLAG(IS_MAC)
void ReportShortIntervalHistograms(
    const char* scenario_suffix,
    const power_metrics::CoalitionResourceUsageRate&
        coalition_resource_usage_rate) {
  for (const char* suffix : {"", scenario_suffix}) {
    UsageTimeHistogram(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", suffix}),
        coalition_resource_usage_rate.cpu_time_per_second, kMaxCPUProportion);
  }
}

void ReportResourceCoalitionHistograms(
    const power_metrics::CoalitionResourceUsageRate& rate,
    const std::vector<const char*>& suffixes) {
  // Calling this function with an empty suffix list is probably a mistake.
  DCHECK(!suffixes.empty());

  // TODO(crbug.com/1229884): Review the units and buckets once we have
  // sufficient data from the field.

  for (const char* scenario_suffix : suffixes) {
    // Suffixes are expected to be empty or starting by a period.
    DCHECK(::strlen(scenario_suffix) == 0U || scenario_suffix[0] == '.');

    UsageTimeHistogram(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.CPUTime2", scenario_suffix}),
        rate.cpu_time_per_second, kMaxCPUProportion);
    UsageTimeHistogram(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.GPUTime2", scenario_suffix}),
        rate.gpu_time_per_second, kMaxGPUProportion);

    // Report the metrics based on a count (e.g. wakeups) with a millievent/sec
    // granularity. In theory it doesn't make much sense to talk about a
    // milliwakeups but the wakeup rate should ideally be lower than one per
    // second in some scenarios and this will provide more granularity.
    constexpr int kMilliFactor = 1000;
    auto scale_sample = [](double sample) -> int {
      // Round the sample to the nearest integer value.
      return std::roundl(sample * kMilliFactor);
    };
    base::UmaHistogramCounts1M(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.InterruptWakeupsPerSecond",
             scenario_suffix}),
        scale_sample(rate.interrupt_wakeups_per_second));
    base::UmaHistogramCounts1M(
        base::StrCat({"PerformanceMonitor.ResourceCoalition."
                      "PlatformIdleWakeupsPerSecond",
                      scenario_suffix}),
        scale_sample(rate.platform_idle_wakeups_per_second));
    base::UmaHistogramCounts10M(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.BytesReadPerSecond2",
             scenario_suffix}),
        rate.bytesread_per_second);
    base::UmaHistogramCounts10M(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.BytesWrittenPerSecond2",
             scenario_suffix}),
        rate.byteswritten_per_second);

    // EnergyImpact is reported in centi-EI, so scaled up by a factor of 100
    // for the histogram recording.
    if (rate.energy_impact_per_second.has_value()) {
      constexpr double kEnergyImpactScalingFactor = 100.0;
      base::UmaHistogramCounts100000(
          base::StrCat({"PerformanceMonitor.ResourceCoalition.EnergyImpact",
                        scenario_suffix}),
          std::roundl(rate.energy_impact_per_second.value() *
                      kEnergyImpactScalingFactor));
    }

    // As of Feb 2, 2022, the value of `rate->power_nw` is always zero on Intel.
    // Don't report it to avoid polluting the data.
    if (base::mac::GetCPUType() == base::mac::CPUType::kArm) {
      constexpr int kMilliWattPerWatt = 1000;
      constexpr int kNanoWattPerMilliWatt = 1000 * 1000;
      // The maximum is 10 watts, which is larger than the 99.99th percentile
      // as of Feb 2, 2022.
      base::UmaHistogramCustomCounts(
          base::StrCat(
              {"PerformanceMonitor.ResourceCoalition.Power2", scenario_suffix}),
          std::roundl(rate.power_nw / kNanoWattPerMilliWatt),
          /* min=*/1, /* exclusive_max=*/10 * kMilliWattPerWatt,
          /* buckets=*/50);
    }

    auto record_qos_level = [&](size_t index, const char* qos_suffix) {
      UsageTimeHistogram(
          base::StrCat({"PerformanceMonitor.ResourceCoalition.QoSLevel.",
                        qos_suffix, scenario_suffix}),
          rate.qos_time_per_second[index], kMaxCPUProportion);
    };

    record_qos_level(THREAD_QOS_DEFAULT, "Default");
    record_qos_level(THREAD_QOS_MAINTENANCE, "Maintenance");
    record_qos_level(THREAD_QOS_BACKGROUND, "Background");
    record_qos_level(THREAD_QOS_UTILITY, "Utility");
    record_qos_level(THREAD_QOS_LEGACY, "Legacy");
    record_qos_level(THREAD_QOS_USER_INITIATED, "UserInitiated");
    record_qos_level(THREAD_QOS_USER_INTERACTIVE, "UserInteractive");
  }
}
#endif  // BUILDFLAG(IS_MAC)
