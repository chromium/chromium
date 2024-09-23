// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/fake_power_status.h"

namespace ash {

FakePowerStatus::FakePowerStatus() = default;

FakePowerStatus::~FakePowerStatus() = default;

void FakePowerStatus::SetBatteryPercent(double percent) {
  battery_percent_ = percent;
  RequestStatusUpdate();
}

void FakePowerStatus::SetIsBatteryPresent(bool present) {
  is_battery_present_ = present;
  RequestStatusUpdate();
}

void FakePowerStatus::SetIsBatterySaverActive(bool active) {
  is_battery_saver_active_ = active;
  RequestStatusUpdate();
}

void FakePowerStatus::SetIsLinePowerConnected(bool connected) {
  is_line_power_connected_ = connected;
  RequestStatusUpdate();
}

void FakePowerStatus::SetIsUsbChargerConnected(bool connected) {
  is_usb_charger_connected_ = connected;
  RequestStatusUpdate();
}

void FakePowerStatus::SetDefaultState() {
  is_battery_present_ = true;
  is_battery_saver_active_ = false;
  is_line_power_connected_ = false;
  is_usb_charger_connected_ = false;
  RequestStatusUpdate();
}

double FakePowerStatus::GetBatteryPercent() const {
  return battery_percent_;
}

bool FakePowerStatus::IsBatteryPresent() const {
  return is_battery_present_;
}

bool FakePowerStatus::IsBatterySaverActive() const {
  return is_battery_saver_active_;
}

bool FakePowerStatus::IsLinePowerConnected() const {
  return is_line_power_connected_;
}

bool FakePowerStatus::IsUsbChargerConnected() const {
  return is_usb_charger_connected_;
}

}  // namespace ash
