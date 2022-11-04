// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_POWER_MANAGER_CLIENT_CONVERSIONS_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_POWER_MANAGER_CLIENT_CONVERSIONS_H_

#include <string>

#include "ash/webui/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace ash {
namespace diagnostics {

mojom::BatteryState ConvertBatteryStateFromProto(
    power_manager::PowerSupplyProperties::BatteryState battery_state);

mojom::ExternalPowerSource ConvertPowerSourceFromProto(
    power_manager::PowerSupplyProperties::ExternalPower power_source);

// Constructs a time-formatted string representing the amount of time remaining
// to either charge or discharge the battery. If the battery is full or the
// amount of time is unreliable / still being calculated, this returns an
// empty string. Otherwise, the time is returned in DURATION_WIDTH_NARROW
// format.
std::u16string ConstructPowerTime(
    mojom::BatteryState battery_state,
    const power_manager::PowerSupplyProperties& power_supply_props);

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_POWER_MANAGER_CLIENT_CONVERSIONS_H_
