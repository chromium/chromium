// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_sounds_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/system/power/power_status.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

// Percentage-based thresholds for remaining battery charge to play sounds when
// plugging in a charger cable.
constexpr int kMidPercentageForCharging = 16;
constexpr int kNormalPercentageForCharging = 80;

// Percentage-based threshold for remaining battery charge to play sounds when
// battery isn't charging.
constexpr int kWarningPercentageForNoCharging = 15;

// Gets the sound for plugging in a power line at different battery levels.
Sound GetSoundKeyForBatteryLevel(int level) {
  if (level >= kNormalPercentageForCharging)
    return Sound::kChargeHighBattery;

  return level >= kMidPercentageForCharging ? Sound::kChargeMediumBattery
                                            : Sound::kChargeLowBattery;
}

PrefService* GetActivePrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

bool GetChargingSoundsEnabled() {
  PrefService* prefs = GetActivePrefService();
  return prefs && prefs->GetBoolean(prefs::kChargingSoundsEnabled);
}

bool GetLowBatterySoundEnabled() {
  PrefService* prefs = GetActivePrefService();
  return prefs && prefs->GetBoolean(prefs::kLowBatterySoundEnabled);
}

}  // namespace

// static
const char PowerSoundsController::kPluggedInBatteryLevelHistogramName[] =
    "Ash.PowerSoundsController.PluggedInBatteryLevel";

// static
const char PowerSoundsController::kUnpluggedBatteryLevelHistogramName[] =
    "Ash.PowerSoundsController.UnpluggedBatteryLevel";

PowerSoundsController::PowerSoundsController() {
  chromeos::PowerManagerClient* client = chromeos::PowerManagerClient::Get();
  DCHECK(client);
  client->AddObserver(this);

  // Get the initial lid state.
  client->GetSwitchStates(
      base::BindOnce(&PowerSoundsController::OnReceiveSwitchStates,
                     weak_factory_.GetWeakPtr()));

  PowerStatus* power_status = PowerStatus::Get();
  power_status->AddObserver(this);

  battery_level_ = power_status->GetRoundedBatteryPercent();
  is_line_power_connected_ = power_status->IsLinePowerConnected();
}

PowerSoundsController::~PowerSoundsController() {
  PowerStatus::Get()->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void PowerSoundsController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kChargingSoundsEnabled,
                                /*default_value=*/false);
  registry->RegisterBooleanPref(prefs::kLowBatterySoundEnabled,
                                /*default_value=*/true);
}

void PowerSoundsController::OnPowerStatusChanged() {
  const PowerStatus& status = *PowerStatus::Get();

  SetPowerStatus(status.GetRoundedBatteryPercent(),
                 status.IsLinePowerConnected(), status.IsBatteryCharging());
}

void PowerSoundsController::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks timestamp) {
  lid_state_ = state;
}

void PowerSoundsController::OnReceiveSwitchStates(
    absl::optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
  if (switch_states.has_value()) {
    lid_state_ = switch_states->lid_state;
  }
}

bool PowerSoundsController::CanPlaySounds() const {
  // Do not play any sound if the device is in DND mode, or if the lid is not
  // open.
  return !message_center::MessageCenter::Get()->IsQuietMode() &&
         lid_state_ == chromeos::PowerManagerClient::LidState::OPEN;
}

void PowerSoundsController::SetPowerStatus(int battery_level,
                                           bool is_line_power_connected,
                                           bool is_battery_charging) {
  const int old_battery_level = battery_level_;
  const bool old_line_power_connected = is_line_power_connected_;

  battery_level_ = battery_level;
  is_line_power_connected_ = is_line_power_connected;

  // Records the battery level only for the device plugged in or Unplugged.
  if (old_line_power_connected != is_line_power_connected) {
    base::UmaHistogramPercentage(is_line_power_connected_
                                     ? kPluggedInBatteryLevelHistogramName
                                     : kUnpluggedBatteryLevelHistogramName,
                                 battery_level_);
  }

  if (!CanPlaySounds())
    return;

  MaybePlaySoundsForCharging(old_line_power_connected);
  MaybePlaySoundsForLowBattery(old_battery_level, is_battery_charging);
}

void PowerSoundsController::MaybePlaySoundsForCharging(
    bool old_line_power_connected) {
  // Don't play the charging sound if the toggle button is disabled by user in
  // the Settings UI.
  if (!GetChargingSoundsEnabled()) {
    return;
  }

  // Returns when it isn't a plug in event.
  bool is_plugging_in = !old_line_power_connected && is_line_power_connected_;
  if (!is_plugging_in)
    return;

  Shell::Get()->system_sounds_delegate()->Play(
      GetSoundKeyForBatteryLevel(battery_level_));
}

void PowerSoundsController::MaybePlaySoundsForLowBattery(
    int old_battery_level,
    bool is_battery_charging) {
  // Don't play the low battery sound if the user turns off the toggle button in
  // the Settings UI.
  if (!GetLowBatterySoundEnabled()) {
    return;
  }

  // Don't play the warning sound if the battery is charging.
  if (is_battery_charging)
    return;

  // We only play sounds during the first time when the battery level drops
  // below `kWarningPercentageForNoCharging` when there is no charging.
  const bool is_warning_battery_level =
      (old_battery_level > kWarningPercentageForNoCharging) &&
      (battery_level_ <= kWarningPercentageForNoCharging);

  if (!is_warning_battery_level)
    return;

  Shell::Get()->system_sounds_delegate()->Play(Sound::kNoChargeLowBattery);
}

}  // namespace ash
