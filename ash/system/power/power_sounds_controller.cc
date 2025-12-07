// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_sounds_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/system/power/battery_saver_controller.h"
#include "ash/system/power/power_status.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "chromeos/ash/components/audio/system_sounds_delegate.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

constexpr ExternalPower kAcPower =
    power_manager::PowerSupplyProperties_ExternalPower_AC;
constexpr ExternalPower kUsbPower =
    power_manager::PowerSupplyProperties_ExternalPower_USB;

// Percentage-based thresholds for remaining battery charge to play sounds when
// plugging in a charger cable.
constexpr int kMidPercentageForCharging = 16;
constexpr int kNormalPercentageForCharging = 80;

// Percentage-based threshold for remaining battery level when using a low-power
// charger.
constexpr int kCriticalWarningPercentage = 5;
constexpr int kLowPowerWarningPercentage = 10;

// Time-based threshold for remaining time when disconnected with the line
// power (any type of charger).
constexpr base::TimeDelta kCriticalWarningMinutes = base::Minutes(5);
constexpr base::TimeDelta kLowPowerWarningMinutes = base::Minutes(15);

// Gets the sound for plugging in an AC charger at different battery levels.
Sound GetSoundKeyForBatteryLevel(int level) {
  if (level >= kNormalPercentageForCharging)
    return Sound::kChargeHighBattery;

  const int threshold =
      IsBatterySaverAllowed()
          ? features::kBatterySaverActivationChargePercent.Get() + 1
          : kMidPercentageForCharging;

  return level >= threshold ? Sound::kChargeMediumBattery
                            : Sound::kChargeLowBattery;
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
  is_ac_charger_connected_ = power_status->IsMainsChargerConnected();

  local_state_ = Shell::Get()->local_state();

  // `local_state_` could be null in tests.
  if (local_state_) {
    low_battery_sound_enabled_.Init(prefs::kLowBatterySoundEnabled,
                                    local_state_);
    charging_sounds_enabled_.Init(prefs::kChargingSoundsEnabled, local_state_);
  }
}

PowerSoundsController::~PowerSoundsController() {
  PowerStatus::Get()->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void PowerSoundsController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kChargingSoundsEnabled,
                                /*default_value=*/false);
  registry->RegisterBooleanPref(prefs::kLowBatterySoundEnabled,
                                /*default_value=*/false);
}

void PowerSoundsController::OnPowerStatusChanged() {
  if (!local_state_) {
    CHECK_IS_TEST();
    return;
  }

  const PowerStatus& status = *PowerStatus::Get();
  SetPowerStatus(status.GetRoundedBatteryPercent(),
                 status.IsBatteryTimeBeingCalculated(), status.external_power(),
                 status.GetBatteryTimeToEmpty());
}

void PowerSoundsController::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks timestamp) {
  lid_state_ = state;
}

void PowerSoundsController::OnReceiveSwitchStates(
    std::optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
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

void PowerSoundsController::SetPowerStatus(
    int battery_level,
    bool is_calculating_battery_time,
    ExternalPower external_power,
    std::optional<base::TimeDelta> remaining_time) {
  battery_level_ = battery_level;

  const bool old_ac_charger_connected = is_ac_charger_connected_;
  is_ac_charger_connected_ = external_power == kAcPower;

  // Records the battery level only for the device plugged in or Unplugged.
  if (old_ac_charger_connected != is_ac_charger_connected_) {
    base::UmaHistogramPercentage(is_ac_charger_connected_
                                     ? kPluggedInBatteryLevelHistogramName
                                     : kUnpluggedBatteryLevelHistogramName,
                                 battery_level_);
  }

  MaybePlaySoundsForCharging(old_ac_charger_connected);

  if (UpdateBatteryState(is_calculating_battery_time, external_power,
                         remaining_time) &&
      ShouldPlayLowBatterySound()) {
    Shell::Get()->system_sounds_delegate()->Play(Sound::kNoChargeLowBattery);
  }
}

void PowerSoundsController::MaybePlaySoundsForCharging(
    bool old_ac_charger_connected) {
  // Don't play the charging sound if the toggle button is disabled by user in
  // the Settings UI.
  if (!charging_sounds_enabled_.GetValue() || !CanPlaySounds()) {
    return;
  }

  // Returns when it isn't a plug in event.
  bool is_plugging_in = !old_ac_charger_connected && is_ac_charger_connected_;
  if (!is_plugging_in)
    return;

  Shell::Get()->system_sounds_delegate()->Play(
      GetSoundKeyForBatteryLevel(battery_level_));
}

bool PowerSoundsController::ShouldPlayLowBatterySound() const {
  if (!low_battery_sound_enabled_.GetValue() || !CanPlaySounds()) {
    return false;
  }

  return current_state_ == BatteryState::kCriticalPower ||
         current_state_ == BatteryState::kLowPower;
}

bool PowerSoundsController::UpdateBatteryState(
    bool is_calculating_battery_time,
    ExternalPower external_power,
    std::optional<base::TimeDelta> remaining_time) {
  const auto new_state = CalculateBatteryState(is_calculating_battery_time,
                                               external_power, remaining_time);
  if (new_state == current_state_) {
    return false;
  }

  current_state_ = new_state;
  return true;
}

PowerSoundsController::BatteryState
PowerSoundsController::CalculateBatteryState(
    bool is_calculating_battery_time,
    ExternalPower external_power,
    std::optional<base::TimeDelta> remaining_time) const {
  const bool is_battery_saver_allowed = IsBatterySaverAllowed();

  if ((is_calculating_battery_time && !is_battery_saver_allowed) ||
      is_ac_charger_connected_) {
    return BatteryState::kNone;
  }

  // The battery state calculation should follow the same logic used by the
  // power notification (Please see
  // `PowerNotificationController::UpdateNotificationState()`). Hence, when a
  // low-power charger (i.e. a USB charger) is connected, or we are using
  // battery saver notifications, we calculate the state based on the remaining
  // `battery_level_` percentage. GetBatteryStateFromBatteryLevel()
  // automatically reflects this differentiation in its logic. Otherwise, when
  // the device is disconnected, we calculate it based on the remaining time
  // until the battery is empty.
  if (is_battery_saver_allowed || external_power == kUsbPower) {
    return GetBatteryStateFromBatteryLevel();
  }
  return GetBatteryStateFromRemainingTime(remaining_time);
}

PowerSoundsController::BatteryState
PowerSoundsController::GetBatteryStateFromBatteryLevel() const {
  if (battery_level_ <= kCriticalWarningPercentage) {
    return BatteryState::kCriticalPower;
  }

  const int low_power_warning_percentage =
      IsBatterySaverAllowed()
          ? features::kBatterySaverActivationChargePercent.Get()
          : kLowPowerWarningPercentage;

  if (battery_level_ <= low_power_warning_percentage) {
    return BatteryState::kLowPower;
  }

  return BatteryState::kNone;
}

PowerSoundsController::BatteryState
PowerSoundsController::GetBatteryStateFromRemainingTime(
    std::optional<base::TimeDelta> remaining_time) const {
  if (!remaining_time) {
    return BatteryState::kNone;
  }

  if (*remaining_time <= kCriticalWarningMinutes) {
    return BatteryState::kCriticalPower;
  }

  if (*remaining_time <= kLowPowerWarningMinutes) {
    return BatteryState::kLowPower;
  }

  return BatteryState::kNone;
}

}  // namespace ash
