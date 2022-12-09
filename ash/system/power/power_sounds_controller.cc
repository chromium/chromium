// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_sounds_controller.h"

#include "ash/shell.h"
#include "ash/system/power/power_status.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/audio/sounds.h"
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

// Returns true if the device can play sounds.
bool CanPlaySounds() {
  // Do not play any sound if the device is in Focus mode, in DND mode.
  // TODO(hongyulong): When Focus mode is available, we need to add this
  // condition here.
  return !message_center::MessageCenter::Get()->IsQuietMode();
}

}  // namespace

// static
const char PowerSoundsController::kPluggedInBatteryLevelHistogramName[] =
    "Ash.PowerSoundsController.PluggedInBatteryLevel";

// static
const char PowerSoundsController::kUnpluggedBatteryLevelHistogramName[] =
    "Ash.PowerSoundsController.UnpluggedBatteryLevel";

PowerSoundsController::PowerSoundsController() {
  PowerStatus* power_status = PowerStatus::Get();
  power_status->AddObserver(this);

  battery_level_ = power_status->GetRoundedBatteryPercent();
  is_line_power_connected_ = power_status->IsLinePowerConnected();
}

PowerSoundsController::~PowerSoundsController() {
  PowerStatus::Get()->RemoveObserver(this);
}

void PowerSoundsController::OnPowerStatusChanged() {
  const PowerStatus& status = *PowerStatus::Get();

  SetPowerStatus(status.GetRoundedBatteryPercent(),
                 status.IsLinePowerConnected(), status.IsBatteryCharging());
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
