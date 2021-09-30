// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/quick_pair/public/cpp/battery_notification.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace quick_pair {

BatteryInfo::BatteryInfo() = default;

BatteryInfo::BatteryInfo(bool is_charging)
    : is_charging(is_charging), percentage(absl::nullopt) {}

BatteryInfo::BatteryInfo(bool is_charging, int8_t percentage)
    : is_charging(is_charging), percentage(percentage) {}

BatteryInfo::BatteryInfo(const BatteryInfo&) = default;

BatteryInfo::BatteryInfo(BatteryInfo&&) = default;

BatteryInfo& BatteryInfo::operator=(const BatteryInfo&) = default;

BatteryInfo& BatteryInfo::operator=(BatteryInfo&&) = default;

BatteryInfo::~BatteryInfo() = default;

BatteryNotification::BatteryNotification() = default;

BatteryNotification::BatteryNotification(bool show_ui,
                                         BatteryInfo left_bud_info,
                                         BatteryInfo right_bud_info,
                                         BatteryInfo case_info)
    : show_ui(show_ui),
      left_bud_info(std::move(left_bud_info)),
      right_bud_info(std::move(right_bud_info)),
      case_info(std::move(case_info)) {}

BatteryNotification::BatteryNotification(const BatteryNotification&) = default;

BatteryNotification::BatteryNotification(BatteryNotification&&) = default;

BatteryNotification& BatteryNotification::operator=(
    const BatteryNotification&) = default;

BatteryNotification& BatteryNotification::operator=(BatteryNotification&&) =
    default;

BatteryNotification::~BatteryNotification() = default;

}  // namespace quick_pair
}  // namespace ash
