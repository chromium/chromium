// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/metrics/power/process_metrics_recorder_util.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"

namespace {

constexpr const char* kBatteryDischargeRateMilliwattsHistogramName =
    "Power.BatteryDischargeRateMilliwatts6";
constexpr const char* kBatteryDischargeRateRelativeHistogramName =
    "Power.BatteryDischargeRateRelative5";
constexpr const char* kBatteryDischargeModeHistogramName =
    "Power.BatteryDischargeMode5";
#if BUILDFLAG(IS_WIN)
constexpr const char* kHasPreciseBatteryDischargeGranularity =
    "Power.HasPreciseBatteryDischargeGranularity";
constexpr const char* kBatteryDischargeRatePreciseMilliwattsHistogramName =
    "Power.BatteryDischargeRatePreciseMilliwatts";
constexpr const char* kBatteryDischargeRateMilliwattsTenMinutesHistogramName =
    "Power.BatteryDischargeRateMilliwatts6.TenMinutes";
constexpr const char* kBatteryDischargeModeTenMinutesHistogramName =
    "Power.BatteryDischargeMode5.TenMinutes";
#endif  // BUILDFLAG(IS_WIN)

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

int64_t CalculateDischargeRateMilliwatts(
    const base::BatteryLevelProvider::BatteryState& previous_battery_state,
    const base::BatteryLevelProvider::BatteryState& new_battery_state,
    base::TimeDelta interval_duration) {
  DCHECK(previous_battery_state.charge_unit.has_value());
  DCHECK(new_battery_state.charge_unit.has_value());
  DCHECK_EQ(previous_battery_state.charge_unit.value(),
            new_battery_state.charge_unit.value());

  const int64_t discharge_capacity =
      (new_battery_state.full_charged_capacity.value() -
       new_battery_state.current_capacity.value()) -
      (previous_battery_state.full_charged_capacity.value() -
       previous_battery_state.current_capacity.value());

  const int64_t discharge_capacity_mwh = [&]() -> int64_t {
    if (new_battery_state.charge_unit.value() ==
        base::BatteryLevelProvider::BatteryLevelUnit::kMWh) {
      return discharge_capacity;
    }

    DCHECK_EQ(new_battery_state.charge_unit.value(),
              base::BatteryLevelProvider::BatteryLevelUnit::kMAh);
    const uint64_t average_mv = (previous_battery_state.voltage_mv.value() +
                                 new_battery_state.voltage_mv.value()) /
                                2;
    return discharge_capacity * average_mv / 1000;
  }();

  // The capacity is in mWh. Divide by hours to get mW. Note that there is no
  // InHoursF() method.
  const double interval_duration_in_hours =
      interval_duration.InSecondsF() / base::Time::kSecondsPerHour;

  return discharge_capacity_mwh / interval_duration_in_hours;
}

int64_t CalculateDischargeRateRelative(
    const base::BatteryLevelProvider::BatteryState& previous_battery_state,
    const base::BatteryLevelProvider::BatteryState& new_battery_state,
    base::TimeDelta interval_duration) {
  // The battery discharge rate is reported per minute with 1/10000 of full
  // charge resolution.
  static constexpr int64_t kDischargeRateFactor = 10000;

  const double previous_level =
      static_cast<double>(previous_battery_state.current_capacity.value()) /
      previous_battery_state.full_charged_capacity.value();
  const double new_level =
      static_cast<double>(new_battery_state.current_capacity.value()) /
      new_battery_state.full_charged_capacity.value();

  const double interval_duration_in_minutes =
      interval_duration.InSecondsF() / base::Time::kSecondsPerMinute;

  return (previous_level - new_level) * kDischargeRateFactor /
         interval_duration_in_minutes;
}

BatteryDischarge GetBatteryDischargeDuringInterval(
    const std::optional<base::BatteryLevelProvider::BatteryState>&
        previous_battery_state,
    const std::optional<base::BatteryLevelProvider::BatteryState>&
        new_battery_state,
    base::TimeDelta interval_duration) {
  if (!previous_battery_state.has_value() || !new_battery_state.has_value()) {
    return {BatteryDischargeMode::kRetrievalError, std::nullopt};
  }
  if (previous_battery_state->is_external_power_connected !=
          new_battery_state->is_external_power_connected ||
      previous_battery_state->battery_count !=
          new_battery_state->battery_count) {
    return {BatteryDischargeMode::kStateChanged, std::nullopt};
  }
  if (new_battery_state->battery_count == 0) {
    return {BatteryDischargeMode::kNoBattery, std::nullopt};
  }
  if (new_battery_state->is_external_power_connected) {
    return {BatteryDischargeMode::kPluggedIn, std::nullopt};
  }
  if (new_battery_state->battery_count > 1) {
    return {BatteryDischargeMode::kMultipleBatteries, std::nullopt};
  }
  if ((previous_battery_state->charge_unit ==
       base::BatteryLevelProvider::BatteryLevelUnit::kRelative) ||
      (new_battery_state->charge_unit ==
       base::BatteryLevelProvider::BatteryLevelUnit::kRelative)) {
    return {BatteryDischargeMode::kInsufficientResolution, std::nullopt};
  }

  // TODO(crbug.com/40756364): Change CHECK to DCHECK in October 2022 after
  // verifying that there are no crash reports.
  CHECK(previous_battery_state->current_capacity.has_value());
  CHECK(previous_battery_state->full_charged_capacity.has_value());
  CHECK(new_battery_state->current_capacity.has_value());
  CHECK(new_battery_state->full_charged_capacity.has_value());

#if BUILDFLAG(IS_MAC)
  // On MacOS, empirical evidence has shown that right after a full charge, the
  // current capacity stays equal to the maximum capacity for several minutes,
  // despite the fact that power was definitely consumed. Reporting a zero
  // discharge rate for this duration would be misleading.
  if (previous_battery_state->current_capacity ==
      previous_battery_state->full_charged_capacity) {
    return {BatteryDischargeMode::kMacFullyCharged, std::nullopt};
  }
#endif

  if (previous_battery_state->full_charged_capacity.value() == 0 ||
      new_battery_state->full_charged_capacity.value() == 0) {
    return {BatteryDischargeMode::kFullChargedCapacityIsZero, std::nullopt};
  }

  const auto discharge_rate_mw = CalculateDischargeRateMilliwatts(
      *previous_battery_state, *new_battery_state, interval_duration);

#if BUILDFLAG(IS_WIN)
  // The maximum granularity allowed for the following battery discharge value.
  // The bell curve of the battery discharge rate starts at 1000 mW. This
  // correspond to a discharge amount of 1000/60 ~ 17 mWh every 1 minute
  // interval.
  static const int64_t kMaximumGranularityInMilliwattHours = 17;
  std::optional<int64_t> discharge_rate_with_precise_granularity;
  if (previous_battery_state->battery_discharge_granularity.has_value() &&
      previous_battery_state->battery_discharge_granularity.value() <=
          kMaximumGranularityInMilliwattHours &&
      new_battery_state->battery_discharge_granularity.has_value() &&
      new_battery_state->battery_discharge_granularity.value() <=
          kMaximumGranularityInMilliwattHours) {
    discharge_rate_with_precise_granularity = discharge_rate_mw;
  }
#endif  // BUILDFLAG(IS_WIN)

  const auto discharge_rate_relative = CalculateDischargeRateRelative(
      *previous_battery_state, *new_battery_state, interval_duration);

  if (discharge_rate_relative < 0 || discharge_rate_mw < 0) {
    return {BatteryDischargeMode::kBatteryLevelIncreased, std::nullopt};
  }
  return {
    .mode = BatteryDischargeMode::kDischarging,
    .rate_milliwatts = discharge_rate_mw,
#if BUILDFLAG(IS_WIN)
    .rate_milliwatts_with_precise_granularity =
        discharge_rate_with_precise_granularity,
#endif
    .rate_relative = discharge_rate_relative
  };
}

void ReportBatteryHistograms(
    base::TimeDelta interval_duration,
    BatteryDischarge battery_discharge,
    bool is_initial_interval,
    const std::vector<const char*>& scenario_suffixes) {
#if BUILDFLAG(IS_WIN)
  base::UmaHistogramBoolean(
      kHasPreciseBatteryDischargeGranularity,
      battery_discharge.rate_milliwatts_with_precise_granularity.has_value());
#endif  // BUILDFLAG(IS_WIN)

  bool battery_saver_enabled =
      performance_manager::user_tuning::BatterySaverModeManager::
          GetInstance() &&
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance()
          ->IsBatterySaverActive();

  const char* interval_type_suffixes[] = {
      "", is_initial_interval ? ".Initial" : ".Periodic"};
  const char* battery_saver_suffixes[] = {"", battery_saver_enabled
                                                  ? ".BatterySaverEnabled"
                                                  : ".BatterySaverDisabled"};
  for (const char* scenario_suffix : scenario_suffixes) {
    for (const char* interval_type_suffix : interval_type_suffixes) {
      base::UmaHistogramEnumeration(
          base::StrCat({kBatteryDischargeModeHistogramName, scenario_suffix,
                        interval_type_suffix}),
          battery_discharge.mode);
      for (const char* battery_saver_suffix : battery_saver_suffixes) {
        if (battery_discharge.mode == BatteryDischargeMode::kDischarging) {
          DCHECK(battery_discharge.rate_milliwatts.has_value());
          base::UmaHistogramCounts100000(
              base::StrCat({kBatteryDischargeRateMilliwattsHistogramName,
                            scenario_suffix, interval_type_suffix,
                            battery_saver_suffix}),
              *battery_discharge.rate_milliwatts);
#if BUILDFLAG(IS_WIN)
          if (battery_discharge.rate_milliwatts_with_precise_granularity) {
            base::UmaHistogramCounts100000(
                base::StrCat(
                    {kBatteryDischargeRatePreciseMilliwattsHistogramName,
                     scenario_suffix, interval_type_suffix,
                     battery_saver_suffix}),
                *battery_discharge.rate_milliwatts_with_precise_granularity);
          }
#endif  // BUILDFLAG(IS_WIN)
          DCHECK(battery_discharge.rate_relative.has_value());
          base::UmaHistogramCounts1000(
              base::StrCat({kBatteryDischargeRateRelativeHistogramName,
                            scenario_suffix, interval_type_suffix,
                            battery_saver_suffix}),
              *battery_discharge.rate_relative);
        }
      }
    }
  }
}

#if BUILDFLAG(IS_WIN)
void ReportBatteryHistogramsTenMinutesInterval(
    base::TimeDelta interval_duration,
    BatteryDischarge battery_discharge) {
  base::UmaHistogramEnumeration(kBatteryDischargeModeTenMinutesHistogramName,
                                battery_discharge.mode);
  if (battery_discharge.mode == BatteryDischargeMode::kDischarging) {
    DCHECK(battery_discharge.rate_milliwatts.has_value());
    base::UmaHistogramCounts100000(
        kBatteryDischargeRateMilliwattsTenMinutesHistogramName,
        *battery_discharge.rate_milliwatts);
  }
}
#endif  // BUILDFLAG(IS_WIN)
