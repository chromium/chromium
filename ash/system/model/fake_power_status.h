// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_FAKE_POWER_STATUS_H_
#define ASH_SYSTEM_MODEL_FAKE_POWER_STATUS_H_

#include "ash/ash_export.h"
#include "ash/system/power/power_status.h"

namespace ash {

// A fake implementation of `PowerStatus`. Used for mocking in the status
// area internals testing page.
class ASH_EXPORT FakePowerStatus : public PowerStatus {
 public:
  FakePowerStatus();

  FakePowerStatus(const FakePowerStatus&) = delete;
  FakePowerStatus& operator=(const FakePowerStatus&) = delete;

  ~FakePowerStatus() override;

  // Sets whether the battery is present.
  void SetIsBatteryPresent(bool present);

  // Sets whether the battery saver mode is currently active.
  void SetIsBatterySaverActive(bool active);

  // Sets whether line power (including a charger of any type) is connected.
  void SetIsLinePowerConnected(bool connected);

  // Sets whether a USB Charger is connected.
  void SetIsUsbChargerConnected(bool connected);

  // Sets the battery's remaining charge as a value in the range [0.0, 100.0]
  void SetBatteryPercent(double percent);

  // Sets the conditions for the battery to not display any icons (default).
  void SetDefaultState();

  // PowerStatus:
  bool IsBatteryPresent() const override;
  bool IsBatterySaverActive() const override;
  bool IsLinePowerConnected() const override;
  bool IsUsbChargerConnected() const override;
  double GetBatteryPercent() const override;

 private:
  double battery_percent_ = 50;
  bool is_battery_present_ = true;
  bool is_battery_saver_active_ = false;
  bool is_line_power_connected_ = false;
  bool is_usb_charger_connected_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_FAKE_POWER_STATUS_H_
