// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/system_info_util.h"

#include <string>
#include <vector>

#include "ash/public/cpp/power_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_usage_data.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace app_list {
namespace {

namespace healthd = ash::cros_healthd::mojom;

constexpr int kMilliampsInAnAmp = 1000;

const std::string GetMetricNameForSourceType(
    const base::StringPiece source_type) {
  if (source_type == "cpu info") {
    return "Apps.AppList.SystemInfoProvider.CrosHealthdProbeError.CpuInfo";
  }
  if (source_type == "memory info") {
    return "Apps.AppList.SystemInfoProvider.CrosHealthdProbeError.MemoryInfo";
  }
  if (source_type == "battery info") {
    return "Apps.AppList.SystemInfoProvider.CrosHealthdProbeError.BatteryInfo";
  }
  NOTREACHED();
  return "";
}

void EmitCrosHealthdProbeError(const base::StringPiece source_type,
                               healthd::ErrorType error_type) {
  const std::string& metric_name = GetMetricNameForSourceType(source_type);

  // `metric_name` may be empty in which case we do not want a metric send
  // attempted.
  if (metric_name.empty()) {
    LOG(WARNING)
        << "Ignoring request to record metric for ProbeError of error_type: "
        << error_type << " for unknown source_stuct: " << source_type;
    return;
  }

  base::UmaHistogramEnumeration(metric_name, error_type);
}

template <typename TResult, typename TTag>

bool CheckResponse(const TResult& result,
                   TTag expected_tag,
                   base::StringPiece type_name) {
  if (result.is_null()) {
    LOG(ERROR) << type_name << "not found in croshealthd response.";
    return false;
  }

  auto tag = result->which();
  if (tag == TTag::kError) {
    EmitCrosHealthdProbeError(type_name, result->get_error()->type);
    LOG(ERROR) << "Error retrieving " << type_name
               << "from croshealthd: " << result->get_error()->msg;
    return false;
  }

  DCHECK_EQ(tag, expected_tag);

  return true;
}

}  // namespace

void EmitBatteryDataError(BatteryDataError error) {
  base::UmaHistogramEnumeration("Apps.AppList.SystemInfoProvider.Error.Battery",
                                error);
}

healthd::MemoryInfo* GetMemoryInfo(const healthd::TelemetryInfo& info) {
  const healthd::MemoryResultPtr& memory_result = info.memory_result;
  if (!CheckResponse(memory_result, healthd::MemoryResult::Tag::kMemoryInfo,
                     "memory info")) {
    return nullptr;
  }

  return memory_result->get_memory_info().get();
}

const healthd::BatteryInfo* GetBatteryInfo(const healthd::TelemetryInfo& info) {
  const healthd::BatteryResultPtr& battery_result = info.battery_result;
  if (!CheckResponse(battery_result, healthd::BatteryResult::Tag::kBatteryInfo,
                     "battery info")) {
    return nullptr;
  }

  const healthd::BatteryInfo* battery_info =
      battery_result->get_battery_info().get();
  if (battery_info->charge_full == 0) {
    LOG(ERROR) << "charge_full from battery_info should not be zero.";
    EmitBatteryDataError(BatteryDataError::kExpectationNotMet);
    return nullptr;
  }

  // Handle values in battery_info which could cause a SIGFPE. See b/227485637.
  if (isnan(battery_info->charge_full) ||
      isnan(battery_info->charge_full_design) ||
      battery_info->charge_full_design == 0) {
    LOG(ERROR) << "battery_info values could cause SIGFPE crash: { "
               << "charge_full_design: " << battery_info->charge_full_design
               << ", charge_full: " << battery_info->charge_full << " }";
    return nullptr;
  }

  return battery_info;
}

healthd::CpuInfo* GetCpuInfo(const healthd::TelemetryInfo& info) {
  const healthd::CpuResultPtr& cpu_result = info.cpu_result;
  if (!CheckResponse(cpu_result, healthd::CpuResult::Tag::kCpuInfo,
                     "cpu info")) {
    return nullptr;
  }

  return cpu_result->get_cpu_info().get();
}

CpuUsageData CalculateCpuUsage(
    const std::vector<healthd::LogicalCpuInfoPtr>& logical_cpu_infos) {
  CpuUsageData new_usage_data;

  DCHECK_GE(logical_cpu_infos.size(), 1u);
  for (const auto& logical_cpu_ptr : logical_cpu_infos) {
    new_usage_data += CpuUsageData(logical_cpu_ptr->user_time_user_hz,
                                   logical_cpu_ptr->system_time_user_hz,
                                   logical_cpu_ptr->idle_time_user_hz);
  }

  return new_usage_data;
}

void PopulateCpuUsage(CpuUsageData new_cpu_usage_data,
                      CpuUsageData previous_cpu_usage_data,
                      CpuData& cpu_usage) {
  CpuUsageData delta = new_cpu_usage_data - previous_cpu_usage_data;

  const uint64_t total_delta = delta.GetTotalTime();
  if (total_delta == 0) {
    LOG(ERROR) << "Device reported having zero logical CPUs.";
    return;
  }
  cpu_usage.SetPercentUsageUser(100 * delta.GetUserTime() / total_delta);
  cpu_usage.SetPercentUsageSystem(100 * delta.GetSystemTime() / total_delta);
  cpu_usage.SetPercentUsageFree(100 * delta.GetIdleTime() / total_delta);
}

void PopulateAverageCpuTemperature(
    const ash::cros_healthd::mojom::CpuInfo& cpu_info,
    CpuData& cpu_usage) {
  if (cpu_info.temperature_channels.empty()) {
    LOG(ERROR) << "Device reported having 0 temperature channels.";
    return;
  }

  uint32_t cumulative_total = 0;
  for (const auto& temp_channel_ptr : cpu_info.temperature_channels) {
    cumulative_total += temp_channel_ptr->temperature_celsius;
  }

  // Integer division.
  cpu_usage.SetAverageCpuTempCelsius(cumulative_total /
                                     cpu_info.temperature_channels.size());
}

void PopulateAverageScaledClockSpeed(const healthd::CpuInfo& cpu_info,
                                     CpuData& cpu_usage) {
  if (cpu_info.physical_cpus.empty() ||
      cpu_info.physical_cpus[0]->logical_cpus.empty()) {
    LOG(ERROR) << "Device reported having 0 logical CPUs.";
    return;
  }

  uint32_t total_scaled_ghz = 0;
  for (const auto& logical_cpu_ptr : cpu_info.physical_cpus[0]->logical_cpus) {
    total_scaled_ghz += logical_cpu_ptr->scaling_current_frequency_khz;
  }

  // Integer division.
  cpu_usage.SetScalingAverageCurrentFrequencyKhz(
      total_scaled_ghz / cpu_info.physical_cpus[0]->logical_cpus.size());
}

void PopulateBatteryHealth(const healthd::BatteryInfo& battery_info,
                           BatteryHealth& battery_health) {
  battery_health.SetCycleCount(battery_info.cycle_count);

  double charge_full_now_milliamp_hours =
      battery_info.charge_full * kMilliampsInAnAmp;
  double charge_full_design_milliamp_hours =
      battery_info.charge_full_design * kMilliampsInAnAmp;
  battery_health.SetBatteryWearPercentage(
      std::min({(100 * charge_full_now_milliamp_hours /
                 charge_full_design_milliamp_hours),
                100.0}));
}

std::u16string GetBatteryTimeText(base::TimeDelta time_left) {
  int hour = 0;
  int min = 0;
  ash::power_utils::SplitTimeIntoHoursAndMinutes(time_left, &hour, &min);

  std::u16string time_text;
  if (hour == 0 || min == 0) {
    // Display only one unit ("2 hours" or "10 minutes").
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG, time_left);
  }

  return ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG,
                                  -1,  // force hour and minute output
                                  time_left);
}

void PopulatePowerStatus(const power_manager::PowerSupplyProperties& proto,
                         BatteryHealth& battery_health) {
  bool charging = proto.battery_state() ==
                  power_manager::PowerSupplyProperties_BatteryState_CHARGING;
  bool calculating = proto.is_calculating_battery_time();
  int percent =
      ash::power_utils::GetRoundedBatteryPercent(proto.battery_percent());
  DCHECK(percent <= 100 && percent >= 0);
  base::TimeDelta time_left;
  bool show_time = false;

  if (!calculating) {
    time_left = base::Seconds(charging ? proto.battery_time_to_full_sec()
                                       : proto.battery_time_to_empty_sec());
    show_time = ash::power_utils::ShouldDisplayBatteryTime(time_left);
  }

  std::u16string status_text;
  std::u16string accessibility_string;
  if (show_time) {
    status_text = l10n_util::GetStringFUTF16(
        charging ? IDS_ASH_BATTERY_STATUS_CHARGING_IN_LAUNCHER_DESCRIPTION_LEFT
                 : IDS_ASH_BATTERY_STATUS_IN_LAUNCHER_DESCRIPTION_LEFT,
        base::NumberToString16(percent), GetBatteryTimeText(time_left));
    accessibility_string = l10n_util::GetStringFUTF16(
        charging
            ? IDS_ASH_BATTERY_STATUS_CHARGING_IN_LAUNCHER_ACCESSIBILITY_LABEL
            : IDS_ASH_BATTERY_STATUS_IN_LAUNCHER_ACCESSIBILITY_LABEL,
        base::NumberToString16(percent), GetBatteryTimeText(time_left));
  } else {
    status_text = l10n_util::GetStringFUTF16(
        IDS_ASH_BATTERY_STATUS_IN_LAUNCHER_DESCRIPTION_LEFT_SHORT,
        base::NumberToString16(percent));
    accessibility_string = l10n_util::GetStringFUTF16(
        IDS_ASH_BATTERY_STATUS_IN_LAUNCHER_ACCESSIBILITY_LABEL_SHORT,
        base::NumberToString16(percent));
  }

  battery_health.SetPowerTime(status_text);
  battery_health.SetAccessibilityLabel(accessibility_string);
  battery_health.SetBatteryPercentage(percent);
}

std::vector<SystemInfoKeywordInput> GetSystemInfoKeywordVector() {
  return {
      SystemInfoKeywordInput(
          SystemInfoInputType::kVersion,
          l10n_util::GetStringUTF16(IDS_ASH_VERSION_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kVersion,
          l10n_util::GetStringUTF16(IDS_ASH_DEVICE_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kVersion,
          l10n_util::GetStringUTF16(IDS_ASH_ABOUT_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kBattery,
          l10n_util::GetStringUTF16(IDS_ASH_BATTERY_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kMemory,
          l10n_util::GetStringUTF16(IDS_ASH_MEMORY_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kMemory,
          l10n_util::GetStringUTF16(IDS_ASH_RAM_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kCPU,
          l10n_util::GetStringUTF16(
              IDS_ASH_ACTIVITY_MONITOR_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kStorage,
          l10n_util::GetStringUTF16(IDS_ASH_STORAGE_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kCPU,
          l10n_util::GetStringUTF16(IDS_ASH_CPU_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kCPU,
          l10n_util::GetStringUTF16(IDS_ASH_DEVICE_SLOW_KEYWORD_FOR_LAUNCHER))};
}

}  // namespace app_list
