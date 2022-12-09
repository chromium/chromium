// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_SOUNDS_CONTROLLER_H_
#define ASH_SYSTEM_POWER_POWER_SOUNDS_CONTROLLER_H_

#include "ash/system/power/power_status.h"

namespace ash {

// Controller class to manage power/battery sounds.
class ASH_EXPORT PowerSoundsController : public PowerStatus::Observer {
 public:
  static const char kPluggedInBatteryLevelHistogramName[];
  static const char kUnpluggedBatteryLevelHistogramName[];

  PowerSoundsController();
  PowerSoundsController(const PowerSoundsController&) = delete;
  PowerSoundsController& operator=(const PowerSoundsController&) = delete;
  ~PowerSoundsController() override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

 private:
  friend class PowerSoundsControllerTest;

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
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_SOUNDS_CONTROLLER_H_
