// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_SMART_CHARGING_SMART_CHARGING_UKM_LOGGER_H_
#define CHROME_BROWSER_ASH_POWER_SMART_CHARGING_SMART_CHARGING_UKM_LOGGER_H_

#include "base/time/time.h"
#include "chromeos/dbus/power_manager/charge_history_state.pb.h"
#include "chromeos/dbus/power_manager/user_charging_event.pb.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ash {
namespace power {

class SmartChargingUkmLogger {
 public:
  SmartChargingUkmLogger() = default;
  ~SmartChargingUkmLogger() = default;
  SmartChargingUkmLogger(const SmartChargingUkmLogger&) = delete;
  SmartChargingUkmLogger& operator=(const SmartChargingUkmLogger&) = delete;

  void LogEvent(const power_manager::UserChargingEvent& user_charging_event,
                const power_manager::ChargeHistoryState& charge_history,
                base::Time time_of_call) const;
};
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_SMART_CHARGING_SMART_CHARGING_UKM_LOGGER_H_
