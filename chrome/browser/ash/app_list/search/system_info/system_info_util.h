// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_UTIL_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/system_info/battery_health.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_data.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_usage_data.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_keyword_input.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace app_list {

// The enums below are used in histograms, do not remove/renumber entries. If
// you're adding to any of these enums, update the corresponding enum listing in
// tools/metrics/histograms/enums.xml: CrosDiagnosticsDataError.
enum class BatteryDataError {
  // Null or nullptr value.
  kNoData = 0,
  // For numeric values that are NaN.
  kNotANumber = 1,
  // Expectation about data not met. Ex. routing prefix is between zero and
  // thirty-two.
  kExpectationNotMet = 2,
  kMaxValue = kExpectationNotMet,
};

void EmitBatteryDataError(BatteryDataError error);

// Extracts MemoryInfo from `info`. Logs and returns a nullptr if MemoryInfo
// in not present.
ash::cros_healthd::mojom::MemoryInfo* GetMemoryInfo(
    const ash::cros_healthd::mojom::TelemetryInfo& info);

// Extracts CpuInfo from `info`. Logs and returns a nullptr if CpuInfo
// in not present.
ash::cros_healthd::mojom::CpuInfo* GetCpuInfo(
    const ash::cros_healthd::mojom::TelemetryInfo& info);

// Extracts BatteryInfo from `info`. Logs and returns a nullptr if
// BatteryInfo in not present.
const ash::cros_healthd::mojom::BatteryInfo* GetBatteryInfo(
    const ash::cros_healthd::mojom::TelemetryInfo& info);

CpuUsageData CalculateCpuUsage(
    const std::vector<ash::cros_healthd::mojom::LogicalCpuInfoPtr>&
        logical_cpu_infos);

void PopulateCpuUsage(CpuUsageData new_cpu_usage_data,
                      CpuUsageData previous_cpu_usage_data,
                      CpuData& cpu_usage);
void PopulateAverageCpuTemperature(
    const ash::cros_healthd::mojom::CpuInfo& cpu_info,
    CpuData& cpu_usage);
void PopulateAverageScaledClockSpeed(
    const ash::cros_healthd::mojom::CpuInfo& cpu_info,
    CpuData& out_cpu_usage);

void PopulateBatteryHealth(
    const ash::cros_healthd::mojom::BatteryInfo& battery_info,
    BatteryHealth& battery_health);

std::u16string CalculatePowerTime(
    const power_manager::PowerSupplyProperties& proto);

void PopulatePowerStatus(
    const power_manager::PowerSupplyProperties& power_supply_properties,
    BatteryHealth& battery_health);

std::u16string GetBatteryTimeText(base::TimeDelta time_left);

std::vector<SystemInfoKeywordInput> GetSystemInfoKeywordVector();

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_UTIL_H_
