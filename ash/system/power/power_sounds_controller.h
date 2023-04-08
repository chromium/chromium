// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_SOUNDS_CONTROLLER_H_
#define ASH_SYSTEM_POWER_POWER_SOUNDS_CONTROLLER_H_

#include "ash/system/power/power_status.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

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

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  // chromeos::PowerManagerClient::Observer:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;

 private:
  friend class PowerSoundsControllerTest;

  // Updates the lid state from received switch states.
  void OnReceiveSwitchStates(
      absl::optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  // Returns true if the device can play sounds.
  bool CanPlaySounds() const;

  void SetPowerStatus(int battery_level,
                      bool line_power_connected,
                      bool is_battery_charging);

  // Plays a sound when any power resource is connected.
  // `old_line_power_connected` records whether line power was connected last
  // time when `OnPowerStatusChanged()` was called.
  void MaybePlaySoundsForCharging(bool old_line_power_connected);

  // Plays a sound when the battery level drops below a certain threshold.
  // `old_battery_level` records the battery level for the last time when
  // `OnPowerStatusChanged()` was called. `is_battery_charging` is true if the
  // battery is charging now.
  void MaybePlaySoundsForLowBattery(int old_battery_level,
                                    bool is_battery_charging);

  // Records the battery level when the `OnPowerStatusChanged()` was called.
  int battery_level_;

  // True if line power is connected when the `OnPowerStatusChanged()` was
  // called.
  bool is_line_power_connected_;

  chromeos::PowerManagerClient::LidState lid_state_ =
      chromeos::PowerManagerClient::LidState::OPEN;

  base::WeakPtrFactory<PowerSoundsController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_SOUNDS_CONTROLLER_H_
