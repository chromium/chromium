// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/battery_answer_result.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_answer_result.h"
#include "chromeos/ash/components/launcher_search/system_info/launcher_util.h"
#include "chromeos/ash/components/system_info/battery_health.h"

namespace app_list {
namespace {

using AnswerCardInfo = ::ash::SystemInfoAnswerCardData;

}  // namespace

BatteryAnswerResult::BatteryAnswerResult(
    Profile* profile,
    const std::u16string& query,
    const std::string& url_path,
    const gfx::ImageSkia& icon,
    double relevance_score,
    const std::u16string& title,
    const std::u16string& description,
    const std::u16string& accessibility_label,
    SystemInfoCategory system_info_category,
    SystemInfoCardType system_info_card_type,
    const AnswerCardInfo& answer_card_info)
    : SystemInfoAnswerResult(profile,
                             query,
                             url_path,
                             icon,
                             relevance_score,
                             title,
                             description,
                             accessibility_label,
                             system_info_category,
                             system_info_card_type,
                             answer_card_info) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

BatteryAnswerResult::~BatteryAnswerResult() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void BatteryAnswerResult::PowerChanged(
    const power_manager::PowerSupplyProperties& power_supply_properties) {
  // The title with the battery time left / to charge will not be updated until
  // this time has been calculated. At this point the answer card will be
  // updated.
  bool calculating = power_supply_properties.is_calculating_battery_time();
  if (calculating) {
    return;
  }
  std::unique_ptr<system_info::BatteryHealth> new_battery_health =
      std::make_unique<system_info::BatteryHealth>();
  launcher_search::PopulatePowerStatus(power_supply_properties,
                                       *new_battery_health.get());
  UpdateTitleAndDetails(/*title=*/std::u16string(),
                        new_battery_health->GetPowerTime(),
                        new_battery_health->GetAccessibilityLabel());
  UpdateBarChartPercentage(new_battery_health->GetBatteryPercentage());
}
}  // namespace app_list
