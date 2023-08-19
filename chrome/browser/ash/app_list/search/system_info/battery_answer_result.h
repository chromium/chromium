// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_BATTERY_ANSWER_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_BATTERY_ANSWER_RESULT_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_answer_result.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace app_list {

class BatteryAnswerResult : public SystemInfoAnswerResult,
                            public chromeos::PowerManagerClient::Observer {
 public:
  BatteryAnswerResult(Profile* profile,
                      const std::u16string& query,
                      const std::string& url_path,
                      const gfx::ImageSkia& icon,
                      double relevance_score,
                      const std::u16string& title,
                      const std::u16string& description,
                      const std::u16string& accessibility_label,
                      SystemInfoCategory system_info_category,
                      SystemInfoCardType system_info_card_type,
                      const ash::SystemInfoAnswerCardData& answer_card_info);

  ~BatteryAnswerResult() override;

  BatteryAnswerResult(const BatteryAnswerResult& other) = delete;
  BatteryAnswerResult& operator=(const BatteryAnswerResult& other) = delete;

  // chromeos::PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties&
                        power_supply_properties) override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_BATTERY_ANSWER_RESULT_H_
