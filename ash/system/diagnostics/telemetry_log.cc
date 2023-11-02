// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/telemetry_log.h"

#include <sstream>
#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

namespace ash {
namespace diagnostics {
namespace {

const char kNewline[] = "\n";

// SystemInfo constants:
const char kSystemInfoSectionName[] = "--- System Info ---";
const char kSystemInfoCurrentTimeTitle[] = "Snapshot Time: ";
const char kSystemInfoBoardNameTitle[] = "Board Name: ";
const char kSystemInfoMarketingNameTitle[] = "Marketing Name: ";
const char kSystemInfoCpuModelNameTitle[] = "CpuModel Name: ";
const char kSystemInfoTotalMemoryTitle[] = "Total Memory (kib): ";
const char kSystemInfoCpuThreadCountTitle[] = "Thread Count:  ";
const char kSystemInfoCpuMaxClockSpeedTitle[] = "Cpu Max Clock Speed (kHz):  ";
const char kSystemInfoMilestoneVersionTitle[] = "Version: ";
const char kSystemInfoHasBatteryTitle[] = "Has Battery: ";

// BatteryChargeStatus constants:
const char kBatteryChargeStatusSectionName[] = "--- Battery Charge Status ---";
const char kBatteryChargeStatusBatteryStateTitle[] = "Battery State: ";
const char kBatteryChargeStatusPowerSourceTitle[] = "Power Source: ";
const char kBatteryChargeStatusPowerTimeTitle[] = "Power Time: ";
const char kBatteryChargeStatusCurrentNowTitle[] = "Current Now (mA): ";
const char kBatteryChargeStatusChargeNowTitle[] = "Chage Now (mAh): ";

// BatteryHealth constants:
const char kBatteryHealthSectionName[] = "--- Battery Health ---";
const char kBatteryHealthChargeFullNowTitle[] = "Charge Full Now (mAh): ";
const char kBatteryHealthChargeFullDesignTitle[] = "Charge Full Design (mAh): ";
const char kBatteryHealthCycleCountTitle[] = "Cycle Count: ";
const char kBatteryHealthWearPercentageTitle[] = "Wear Percentage: ";

// MemoryUsage constants:
const char kMemoryUsageSectionName[] = "--- Memory Usage ---";
const char kMemoryUsageTotalMemoryTitle[] = "Total Memory (kib): ";
const char kMemoryUsageAvailableMemoryTitle[] = "Available Memory (kib): ";
const char kMemoryUsageFreeMemoryTitle[] = "Free Memory (kib): ";

// CpuUsage constants:
const char kCpuUsageSectionName[] = "--- Cpu Usage ---";
const char kCpuUsageUserTitle[] = "Usage User (%): ";
const char kCpuUsageSystemTitle[] = "Usage System (%): ";
const char kCpuUsageFreeTitle[] = "Usage Free (%): ";
const char kCpuUsageAvgTempTitle[] = "Avg Temp (C): ";
const char kCpuUsageScalingFrequencyTitle[] =
    "Current scaled frequency (kHz): ";

std::string GetCurrentDateTimeWithTimeZoneAsString() {
  return base::UTF16ToUTF8(
      base::TimeFormatShortDateAndTimeWithTimeZone(base::Time::Now()));
}

}  // namespace

TelemetryLog::TelemetryLog() = default;
TelemetryLog::~TelemetryLog() = default;

void TelemetryLog::UpdateSystemInfo(mojom::SystemInfoPtr latest_system_info) {
  latest_system_info_ = std::move(latest_system_info);
}

void TelemetryLog::UpdateBatteryChargeStatus(
    mojom::BatteryChargeStatusPtr latest_battery_charge_status) {
  latest_battery_charge_status_ = std::move(latest_battery_charge_status);
}

void TelemetryLog::UpdateBatteryHealth(
    mojom::BatteryHealthPtr latest_battery_health) {
  latest_battery_health_ = std::move(latest_battery_health);
}

void TelemetryLog::UpdateMemoryUsage(
    mojom::MemoryUsagePtr latest_memory_usage) {
  latest_memory_usage_ = std::move(latest_memory_usage);
}

void TelemetryLog::UpdateCpuUsage(mojom::CpuUsagePtr latest_cpu_usage) {
  latest_cpu_usage_ = std::move(latest_cpu_usage);
}

std::string TelemetryLog::GetContents() const {
  std::stringstream output;
  if (latest_system_info_) {
    output << kSystemInfoSectionName << kNewline << kSystemInfoCurrentTimeTitle
           << GetCurrentDateTimeWithTimeZoneAsString() << kNewline
           << kSystemInfoBoardNameTitle << latest_system_info_->board_name
           << kNewline << kSystemInfoMarketingNameTitle
           << latest_system_info_->marketing_name << kNewline
           << kSystemInfoCpuModelNameTitle
           << latest_system_info_->cpu_model_name << kNewline
           << kSystemInfoTotalMemoryTitle
           << latest_system_info_->total_memory_kib << kNewline
           << kSystemInfoCpuThreadCountTitle
           << latest_system_info_->cpu_threads_count << kNewline
           << kSystemInfoCpuMaxClockSpeedTitle
           << latest_system_info_->cpu_max_clock_speed_khz << kNewline
           << kSystemInfoMilestoneVersionTitle
           << latest_system_info_->version_info->full_version_string << kNewline
           << kSystemInfoHasBatteryTitle
           << ((latest_system_info_->device_capabilities->has_battery)
                   ? "true"
                   : "false")
           << kNewline << kNewline;
  }
  if (latest_battery_charge_status_) {
    output << kBatteryChargeStatusSectionName << kNewline
           << kBatteryChargeStatusBatteryStateTitle
           << latest_battery_charge_status_->battery_state << kNewline
           << kBatteryChargeStatusPowerSourceTitle
           << latest_battery_charge_status_->power_adapter_status << kNewline
           << kBatteryChargeStatusPowerTimeTitle
           << latest_battery_charge_status_->power_time << kNewline
           << kBatteryChargeStatusCurrentNowTitle
           << latest_battery_charge_status_->current_now_milliamps << kNewline
           << kBatteryChargeStatusChargeNowTitle
           << latest_battery_charge_status_->charge_now_milliamp_hours
           << kNewline << kNewline;
  }
  if (latest_battery_health_) {
    output << kBatteryHealthSectionName << kNewline
           << kBatteryHealthChargeFullNowTitle
           << latest_battery_health_->charge_full_now_milliamp_hours << kNewline
           << kBatteryHealthChargeFullDesignTitle
           << latest_battery_health_->charge_full_design_milliamp_hours
           << kNewline << kBatteryHealthCycleCountTitle
           << latest_battery_health_->cycle_count << kNewline
           << kBatteryHealthWearPercentageTitle
           << base::NumberToString(
                  latest_battery_health_->battery_wear_percentage)
           << kNewline << kNewline;
  }
  if (latest_memory_usage_) {
    output << kMemoryUsageSectionName << kNewline
           << kMemoryUsageTotalMemoryTitle
           << latest_memory_usage_->total_memory_kib << kNewline
           << kMemoryUsageAvailableMemoryTitle
           << latest_memory_usage_->available_memory_kib << kNewline
           << kMemoryUsageFreeMemoryTitle
           << latest_memory_usage_->free_memory_kib << kNewline << kNewline;
  }
  if (latest_cpu_usage_) {
    output << kCpuUsageSectionName << kNewline << kCpuUsageUserTitle
           << base::NumberToString(latest_cpu_usage_->percent_usage_user)
           << kNewline << kCpuUsageSystemTitle
           << base::NumberToString(latest_cpu_usage_->percent_usage_system)
           << kNewline << kCpuUsageFreeTitle
           << base::NumberToString(latest_cpu_usage_->percent_usage_free)
           << kNewline << kCpuUsageAvgTempTitle
           << latest_cpu_usage_->average_cpu_temp_celsius << kNewline
           << kCpuUsageScalingFrequencyTitle
           << latest_cpu_usage_->scaling_current_frequency_khz << kNewline
           << kNewline;
  }

  return output.str();
}

}  // namespace diagnostics
}  // namespace ash
