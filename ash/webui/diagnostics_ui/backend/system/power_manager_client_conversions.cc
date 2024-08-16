// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/system/power_manager_client_conversions.h"

#include "ash/public/cpp/power_utils.h"
#include "base/time/time.h"
#include "ui/base/l10n/time_format.h"

namespace ash {
namespace diagnostics {

mojom::BatteryState ConvertBatteryStateFromProto(
    power_manager::PowerSupplyProperties::BatteryState battery_state) {
  DCHECK_NE(power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT,
            battery_state);

  switch (battery_state) {
    case power_manager::PowerSupplyProperties_BatteryState_CHARGING:
      return mojom::BatteryState::kCharging;
    case power_manager::PowerSupplyProperties_BatteryState_DISCHARGING:
      return mojom::BatteryState::kDischarging;
    case power_manager::PowerSupplyProperties_BatteryState_FULL:
      return mojom::BatteryState::kFull;
    default:
      NOTREACHED();
  }
}

mojom::ExternalPowerSource ConvertPowerSourceFromProto(
    power_manager::PowerSupplyProperties::ExternalPower power_source) {
  switch (power_source) {
    case power_manager::PowerSupplyProperties_ExternalPower_AC:
      return mojom::ExternalPowerSource::kAc;
    case power_manager::PowerSupplyProperties_ExternalPower_USB:
      return mojom::ExternalPowerSource::kUsb;
    case power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED:
      return mojom::ExternalPowerSource::kDisconnected;
    default:
      NOTREACHED();
  }
}

std::u16string ConstructPowerTime(
    mojom::BatteryState battery_state,
    const power_manager::PowerSupplyProperties& power_supply_props) {
  if (battery_state == mojom::BatteryState::kFull) {
    // Return an empty string if the battery is full.
    return std::u16string();
  }

  int64_t time_in_seconds;
  if (battery_state == mojom::BatteryState::kCharging) {
    time_in_seconds = power_supply_props.has_battery_time_to_full_sec()
                          ? power_supply_props.battery_time_to_full_sec()
                          : -1;
  } else {
    DCHECK(battery_state == mojom::BatteryState::kDischarging);
    time_in_seconds = power_supply_props.has_battery_time_to_empty_sec()
                          ? power_supply_props.battery_time_to_empty_sec()
                          : -1;
  }

  if (power_supply_props.is_calculating_battery_time() || time_in_seconds < 0) {
    // If power manager is still calculating battery time or |time_in_seconds|
    // is negative (meaning power manager couldn't compute a reasonable time)
    // return an empty string.
    return std::u16string();
  }

  const base::TimeDelta as_time_delta = base::Seconds(time_in_seconds);

  int hour = 0;
  int min = 0;
  ash::power_utils::SplitTimeIntoHoursAndMinutes(as_time_delta, &hour, &min);

  if (hour == 0 || min == 0) {
    // Display only one unit ("2 hours" or "10 minutes").
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG, as_time_delta);
  }

  return ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG,
                                  -1,  // force hour and minute output
                                  as_time_delta);
}

}  // namespace diagnostics
}  // namespace ash
