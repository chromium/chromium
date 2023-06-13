// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_saver_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/message_center.h"

namespace ash {

const double BatterySaverController::kActivationChargePercent = 20.0;

BatterySaverController::BatterySaverController(PrefService* local_state)
    : local_state_(local_state),
      always_on_(features::IsBatterySaverAlwaysOn()) {
  power_status_observation_.Observe(PowerStatus::Get());

  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      prefs::kPowerBatterySaver,
      base::BindRepeating(&BatterySaverController::OnSettingsPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  // Restore state from the saved preference value.
  OnSettingsPrefChanged();
}

BatterySaverController::~BatterySaverController() = default;

// static
void BatterySaverController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kPowerBatterySaver, false);
}

void BatterySaverController::OnPowerStatusChanged() {
  if (always_on_) {
    SetBatterySaverState(true);
    return;
  }

  auto* power_status = PowerStatus::Get();
  double battery_percent = power_status->GetBatteryPercent();
  bool active = power_status->IsBatterySaverActive();
  bool on_AC_power = power_status->IsMainsChargerConnected();

  // Should we turn off battery saver?
  if (active && on_AC_power) {
    SetBatterySaverState(false);
  }

  if (!active && !on_AC_power && battery_percent <= kActivationChargePercent) {
    SetBatterySaverState(true);
  }
}

void BatterySaverController::OnSettingsPrefChanged() {
  if (always_on_) {
    SetBatterySaverState(true);
    return;
  }

  // OS Settings has changed the pref, tell Power Manager.
  SetBatterySaverState(local_state_->GetBoolean(prefs::kPowerBatterySaver));
}

void BatterySaverController::SetBatterySaverState(bool active) {
  if (active != local_state_->GetBoolean(prefs::kPowerBatterySaver)) {
    local_state_->SetBoolean(prefs::kPowerBatterySaver, active);
  }
  if (active != PowerStatus::Get()->IsBatterySaverActive()) {
    power_manager::SetBatterySaverModeStateRequest request;
    request.set_enabled(active);
    chromeos::PowerManagerClient::Get()->SetBatterySaverModeState(request);
  }
}

}  // namespace ash
