// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_SOUNDS_CONTROLLER_H_
#define ASH_SYSTEM_POWER_POWER_SOUNDS_CONTROLLER_H_

#include "ash/system/power/power_status.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_member.h"

class PrefRegistrySimple;

namespace ash {

namespace {

using ExternalPower = power_manager::PowerSupplyProperties_ExternalPower;

}

// Controller class to manage power/battery sounds.
class ASH_EXPORT PowerSoundsController
    : public PowerStatus::Observer,
      public chromeos::PowerManagerClient::Observer {
 public:
  static const char kPluggedInBatteryLevelHistogramName[];
  static const char kUnpluggedBatteryLevelHistogramName[];

  PowerSoundsController();
  PowerSoundsController(const PowerSoundsController&) = delete;
  PowerSoundsController& operator=(const PowerSoundsController&) = delete;
  ~PowerSoundsController() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  // chromeos::PowerManagerClient::Observer:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;

 private:
  friend class PowerSoundsControllerTest;

  // Type of the battery state according to the critical power or low power
  // threshold.
  enum class BatteryState {
    // The remaining battery level or minutes isn't larger than the critical
    // power threshold.
    kCriticalPower,

    // The remaining battery level or minutes isn't larger than the low power
    // threshold but higher than the critical power threshold.
    kLowPower,

    // Other status. e.g. when connecting with an AC charger, or the remaining
    // battery level larger than the low power threshold.
    kNone,
  };

  // Updates the lid state from received switch states.
  void OnReceiveSwitchStates(
      std::optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  // Returns true if the device can play sounds.
  bool CanPlaySounds() const;

  void SetPowerStatus(int battery_level,
                      bool is_calculating_battery_time,
                      ExternalPower external_power,
                      std::optional<base::TimeDelta> remaining_time);

  // Plays a sound when any power resource is connected.
  // `old_ac_charger_connected` records whether line power was connected last
  // time when `OnPowerStatusChanged()` was called.
  void MaybePlaySoundsForCharging(bool old_ac_charger_connected);

  bool ShouldPlayLowBatterySound() const;

  // Returns true if the `current_state_` will be updated to a new state.
  bool UpdateBatteryState(bool is_calculating_battery_time,
                          ExternalPower external_power,
                          std::optional<base::TimeDelta> remaining_time);

  BatteryState CalculateBatteryState(
      bool is_calculating_battery_time,
      ExternalPower external_power,
      std::optional<base::TimeDelta> remaining_time) const;
  BatteryState GetBatteryStateFromBatteryLevel() const;
  BatteryState GetBatteryStateFromRemainingTime(
      std::optional<base::TimeDelta> remaining_time) const;

  // Records the battery level when the `OnPowerStatusChanged()` was called.
  int battery_level_;

  // True if an AC charger is connected when the `OnPowerStatusChanged()` was
  // called.
  bool is_ac_charger_connected_;

  BatteryState current_state_ = BatteryState::kNone;

  chromeos::PowerManagerClient::LidState lid_state_ =
      chromeos::PowerManagerClient::LidState::OPEN;

  // An observer to listen for changes to prefs::kLowBatterySoundEnabled.
  BooleanPrefMember low_battery_sound_enabled_;

  // An observer to listen for changes to prefs::kChargingSoundsEnabled.
  BooleanPrefMember charging_sounds_enabled_;

  raw_ptr<PrefService> local_state_ =
      nullptr;  // Non-owned and must out-live this.

  base::WeakPtrFactory<PowerSoundsController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_SOUNDS_CONTROLLER_H_
