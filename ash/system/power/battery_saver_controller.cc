// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_saver_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/logging.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
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

void BatterySaverController::SetStateForTesting(bool active) {
  SetBatterySaverState(active);
}

void BatterySaverController::DisplayBatterySaverModeDisabledToast() {
  ToastManager* toast_manager = ToastManager::Get();
  // `toast_manager` can be null when this function is called in the unit tests
  // due to initialization priority.
  if (toast_manager == nullptr) {
    return;
  }

  toast_manager->Show(ToastData(
      "battery_saver_mode_state_changed",
      ToastCatalogName::kBatterySaverDisabled,
      l10n_util::GetStringUTF16(IDS_ASH_BATTERY_SAVER_DISABLED_TOAST_TEXT),
      ToastData::kDefaultToastDuration, true));
}

void BatterySaverController::SetBatterySaverState(bool active) {
  bool changed = false;
  if (active != local_state_->GetBoolean(prefs::kPowerBatterySaver)) {
    local_state_->SetBoolean(prefs::kPowerBatterySaver, active);
    changed = true;
  }
  if (active != PowerStatus::Get()->IsBatterySaverActive()) {
    power_manager::SetBatterySaverModeStateRequest request;
    request.set_enabled(active);
    chromeos::PowerManagerClient::Get()->SetBatterySaverModeState(request);
    changed = true;
  }

  if (changed && !active) {
    DisplayBatterySaverModeDisabledToast();
  }
}

}  // namespace ash
