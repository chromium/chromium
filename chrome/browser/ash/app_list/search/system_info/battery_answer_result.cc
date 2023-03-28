// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/battery_answer_result.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/power_utils.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_answer_result.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_util.h"

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
    SystemInfoCategory system_info_category,
    const AnswerCardInfo& answer_card_info)
    : SystemInfoAnswerResult(profile,
                             query,
                             url_path,
                             icon,
                             relevance_score,
                             title,
                             description,
                             system_info_category,
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
  std::u16string power_time = CalculatePowerTime(power_supply_properties);
  int percent = ash::power_utils::GetRoundedBatteryPercent(
      power_supply_properties.battery_percent());
  AnswerCardInfo answer_card_info(percent);
  UpdateTitle(power_time);
  SetSystemInfoAnswerCardData(answer_card_info);
}
}  // namespace app_list
