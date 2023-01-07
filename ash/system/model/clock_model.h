// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_CLOCK_MODEL_H_
#define ASH_SYSTEM_MODEL_CLOCK_MODEL_H_

#include "base/i18n/time_formatting.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/settings/timezone_settings.h"

namespace ash {

class ClockObserver;

// Model to notify system clock and related configuration change.
class ClockModel : public SystemClockClient::Observer,
                   public system::TimezoneSettings::Observer {
 public:
  ClockModel();

  ClockModel(const ClockModel&) = delete;
  ClockModel& operator=(const ClockModel&) = delete;

  ~ClockModel() override;

  void AddObserver(ClockObserver* observer);
  void RemoveObserver(ClockObserver* observer);

  void SetUse24HourClock(bool use_24_hour);

  // Return true if the session is logged in.
  bool IsLoggedIn() const;

  // Return true if Web UI date time settings or time dialog is available.
  bool IsSettingsAvailable() const;

  void ShowDateSettings();
  void ShowPowerSettings();
  void ShowSetTimeDialog();

  // Force observers to refresh clock views e.g. system is resumed or timezone
  // is changed.
  void NotifyRefreshClock();

  // SystemClockClient::Observer:
  void SystemClockUpdated() override;
  void SystemClockCanSetTimeChanged(bool can_set_time) override;

  // ash::system::TimezoneSettings::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  base::HourClockType hour_clock_type() const { return hour_clock_type_; }

  bool can_set_time() const { return can_set_time_; }

 private:
  void NotifyDateFormatChanged();
  void NotifySystemClockTimeUpdated();
  void NotifySystemClockCanSetTimeChanged(bool can_set_time);

  // The type of clock hour display: 12 or 24 hour.
  base::HourClockType hour_clock_type_ = base::k12HourClock;

  // If system clock can be configured by user through SetTimeDialog.
  bool can_set_time_ = false;

  base::ObserverList<ClockObserver>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_CLOCK_MODEL_H_
