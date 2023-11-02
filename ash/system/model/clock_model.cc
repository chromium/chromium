// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/clock_model.h"

#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/clock_observer.h"
#include "ash/system/model/system_tray_model.h"

namespace ash {

ClockModel::ClockModel() : hour_clock_type_(base::GetHourClockType()) {
  // SystemClockClient may be null in tests.
  if (SystemClockClient::Get()) {
    SystemClockClient::Get()->AddObserver(this);
    can_set_time_ = SystemClockClient::Get()->CanSetTime();
  }
  system::TimezoneSettings::GetInstance()->AddObserver(this);
}

ClockModel::~ClockModel() {
  // SystemClockClient may be null in tests.
  if (SystemClockClient::Get())
    SystemClockClient::Get()->RemoveObserver(this);
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void ClockModel::AddObserver(ClockObserver* observer) {
  observers_.AddObserver(observer);
}

void ClockModel::RemoveObserver(ClockObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ClockModel::SetUse24HourClock(bool use_24_hour) {
  hour_clock_type_ = use_24_hour ? base::k24HourClock : base::k12HourClock;
  NotifyDateFormatChanged();
}

bool ClockModel::IsLoggedIn() const {
  return Shell::Get()->session_controller()->login_status() !=
         LoginStatus::NOT_LOGGED_IN;
}

bool ClockModel::IsSettingsAvailable() const {
  return Shell::Get()->session_controller()->ShouldEnableSettings() ||
         can_set_time();
}

void ClockModel::ShowDateSettings() {
  Shell::Get()->system_tray_model()->client()->ShowDateSettings();
}

void ClockModel::ShowPowerSettings() {
  Shell::Get()->system_tray_model()->client()->ShowPowerSettings();
}

void ClockModel::ShowSetTimeDialog() {
  Shell::Get()->system_tray_model()->client()->ShowSetTimeDialog();
}

void ClockModel::NotifyRefreshClock() {
  for (auto& observer : observers_)
    observer.Refresh();
}

void ClockModel::NotifyDateFormatChanged() {
  for (auto& observer : observers_)
    observer.OnDateFormatChanged();
}

void ClockModel::NotifySystemClockTimeUpdated() {
  for (auto& observer : observers_)
    observer.OnSystemClockTimeUpdated();
}

void ClockModel::NotifySystemClockCanSetTimeChanged(bool can_set_time) {
  for (auto& observer : observers_)
    observer.OnSystemClockCanSetTimeChanged(can_set_time);
}

void ClockModel::SystemClockUpdated() {
  NotifySystemClockTimeUpdated();
}

void ClockModel::SystemClockCanSetTimeChanged(bool can_set_time) {
  can_set_time_ = can_set_time;
  NotifySystemClockCanSetTimeChanged(can_set_time_);
}

void ClockModel::TimezoneChanged(const icu::TimeZone& timezone) {
  NotifyRefreshClock();
}

}  // namespace ash
