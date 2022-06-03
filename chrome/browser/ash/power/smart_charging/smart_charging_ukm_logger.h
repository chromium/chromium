// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_SMART_CHARGING_SMART_CHARGING_UKM_LOGGER_H_
#define CHROME_BROWSER_ASH_POWER_SMART_CHARGING_SMART_CHARGING_UKM_LOGGER_H_

#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ash {
namespace power {
class UserChargingEvent;

class SmartChargingUkmLogger {
 public:
  SmartChargingUkmLogger() = default;
  ~SmartChargingUkmLogger() = default;
  SmartChargingUkmLogger(const SmartChargingUkmLogger&) = delete;
  SmartChargingUkmLogger& operator=(const SmartChargingUkmLogger&) = delete;

  void LogEvent(const UserChargingEvent& user_charging_event) const;
};
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_SMART_CHARGING_SMART_CHARGING_UKM_LOGGER_H_
