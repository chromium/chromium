// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/performance_log_source.h"

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "components/feedback/feedback_report.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

using performance_manager::user_tuning::prefs::BatterySaverModeState;
using performance_manager::user_tuning::prefs::kBatterySaverModeState;

namespace {
std::string BoolToString(bool value) {
  return value ? "true" : "false";
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::string BatterySaverModeStateToString(BatterySaverModeState state) {
  switch (state) {
    case BatterySaverModeState::kDisabled:
      return "disabled";
    case BatterySaverModeState::kEnabledBelowThreshold:
      return "enabled_threshold";
    case BatterySaverModeState::kEnabledOnBattery:
      return "enabled_battery_power";
    case BatterySaverModeState::kEnabled:
      return "enabled";
    default:
      return "unknown_battery_saver_mode_state";
  }
}
#endif  //  !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

PerformanceLogSource::PerformanceLogSource() : SystemLogsSource("Performance") {
  battery_saver_mode_manager_ =
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance();
  tuning_manager_ = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
}

PerformanceLogSource::~PerformanceLogSource() = default;

void PerformanceLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();
  CHECK(tuning_manager_);
  PopulatePerformanceSettingLogs(response.get());
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  PopulateBatteryDetailLogs(response.get());
#endif
  std::move(callback).Run(std::move(response));
}

void PerformanceLogSource::PopulatePerformanceSettingLogs(
    SystemLogsResponse* response) {
  response->emplace("high_efficiency_mode_active",
                    BoolToString(tuning_manager_->IsMemorySaverModeActive()));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Battery and battery saver logs are not used on ChromeOS.
  PrefService* local_prefs = g_browser_process->local_state();
  int battery_saver_state = local_prefs->GetInteger(kBatterySaverModeState);
  bool is_battery_saver_active =
      battery_saver_mode_manager_->IsBatterySaverActive();
  bool is_battery_saver_disabled_for_session =
      battery_saver_mode_manager_->IsBatterySaverModeDisabledForSession();

  response->emplace(
      "battery_saver_state",
      BatterySaverModeStateToString(
          static_cast<BatterySaverModeState>(battery_saver_state)));
  response->emplace("battery_saver_mode_active",
                    BoolToString(is_battery_saver_active));
  response->emplace("battery_saver_disabled_for_session",
                    BoolToString(is_battery_saver_disabled_for_session));
#endif  //  !BUILDFLAG(IS_CHROMEOS_ASH)
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void PerformanceLogSource::PopulateBatteryDetailLogs(
    SystemLogsResponse* response) {
  bool has_battery = battery_saver_mode_manager_->DeviceHasBattery();
  bool is_using_battery_power =
      battery_saver_mode_manager_->IsUsingBatteryPower();
  int battery_percentage =
      battery_saver_mode_manager_->SampledBatteryPercentage();

  response->emplace("device_has_battery", BoolToString(has_battery));
  response->emplace("device_using_battery_power",
                    BoolToString(is_using_battery_power));
  response->emplace("device_battery_percentage",
                    base::NumberToString(battery_percentage));
}
#endif  //  !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace system_logs
