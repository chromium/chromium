// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_OFF_HOURS_DEVICE_OFF_HOURS_CONTROLLER_H_
#define CHROME_BROWSER_ASH_POLICY_OFF_HOURS_DEVICE_OFF_HOURS_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace policy {
namespace off_hours {

// The main class for handling "OffHours" policy turns "OffHours" mode on and
// off, handles server and client time, timezone.
//
// DeviceSettingsService is owner of this object. Use to get a reference:
// DeviceSettingsService::Get()->device_off_hours_controller()
//
// Device setting proto is changed in DeviceSettingsService and doesn't contain
// ignored device policies from DeviceOffHoursProto during "OffHours" mode. It
// is changed exactly in DeviceSettingsService because it's late to change
// policies in PrefValueMap and PolicyMap. The system will revert to the default
// behavior for the removed policies. And behavior of policies is handled during
// decoding process from proto to PolicyMap.
//
// "OffHours" mode requires the system clock to be network synchronized.
// However, network synchronization happens only after the first policy parsing.
// We therefore have to assume that the clock is in fact synchronized at
// startup. Otherwise we would terminate off-hour-allowed guest sessions
// immediately.
class DeviceOffHoursController : public ash::SystemClockClient::Observer {
 public:
  // Observer interface.
  class Observer {
   public:
    // Gets called when "OffHours" end time is changed.
    virtual void OnOffHoursEndTimeChanged() {}

   protected:
    virtual ~Observer() {}
  };

  // Creates a device off hours controller instance.
  DeviceOffHoursController();

  DeviceOffHoursController(const DeviceOffHoursController&) = delete;
  DeviceOffHoursController& operator=(const DeviceOffHoursController&) = delete;

  ~DeviceOffHoursController() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Return current "OffHours" mode status.
  bool is_off_hours_mode() const { return off_hours_mode_; }

  // Return true if the current user session is allowed only during the
  // "OffHours" and will be signed out at the end of session. Always returns
  // false outside of "OffHours".
  bool IsCurrentSessionAllowedOnlyForOffHours() const;

  // Save actual "OffHours" intervals from |device_settings_proto| to
  // |off_hours_intervals_| and call "UpdateOffhoursMode()".
  void UpdateOffHoursPolicy(
      const enterprise_management::ChromeDeviceSettingsProto&
          device_settings_proto);

  // Return "OffHours" mode end time during "OffHours" mode is on. Return null
  // when "OffHours" mode is off.
  base::Time GetOffHoursEndTime() const { return off_hours_end_time_; }

  // ash::SystemClockClient::Observer:
  void SystemClockUpdated() override;

  // |timer_clock| is not owned and its lifetime should cover lifetime of
  // DeviceOffHoursContoller.
  void SetClockForTesting(base::Clock* clock,
                          const base::TickClock* timer_clock);

 private:
  // Run OnOffHoursEndTimeChanged() for observers.
  void NotifyOffHoursEndTimeChanged() const;

  // Called when "OffHours" mode is changed and ask DeviceSettingsService to
  // update current proto.
  void OffHoursModeIsChanged() const;

  // If device should be in "OffHours" mode based on the current time then apply
  // current "OffHours" interval and turn "OffHours" mode on otherwise turn
  // "OffHours" mode off and set timer to next update "OffHours" mode.
  void UpdateOffHoursMode();

  // Turn on and off "OffHours" mode and call "OffHoursModeIsChanged()" if
  // "OffHours" mode is changed.
  void SetOffHoursMode(bool off_hours_enabled);

  // Set "OffHours" mode end time.
  void SetOffHoursEndTime(base::Time off_hours_end_time);

  // Timer for update "OffHours" mode.
  void StartOffHoursTimer(base::Time update_time);
  void StopOffHoursTimer();

  // Called once when the system clock service initially becomes available (or
  // immediately if it's already available).
  void SystemClockInitiallyAvailable(bool service_is_available);

  // Called when the system clock synchronization status with network time is
  // changed.
  void NetworkSynchronizationUpdated(bool network_synchronized);

  base::ObserverList<Observer>::Unchecked observers_;

  // The main value of "OffHours" policy which indicates current "OffHours" mode
  // state.
  bool off_hours_mode_ = false;

  // "OffHours" mode end time. It is needed to show "OffHours" session limit
  // notification. When "OffHours" mode is off the value is base::Time{}.
  base::Time off_hours_end_time_;

  // Timer for updating device settings at the begin of next “OffHours” interval
  // or at the end of current "OffHours" interval.
  std::unique_ptr<base::WallClockTimer> timer_;

  // Used for testing purposes, otherwise it's an instance of
  // base::DefaultClock.
  raw_ptr<base::Clock> clock_;

  // We start with the assumption that the system clock is synchronized with the
  // network time. This will be updated once actual synchronization attempts are
  // made by the system.
  bool is_clock_network_synchronized_ = true;

  // Current "OffHours" time intervals.
  std::vector<WeeklyTimeInterval> off_hours_intervals_;

  // Non-"OffHours" device settings proto.
  // Needed to check policies outside of "OffHours" mode.
  enterprise_management::ChromeDeviceSettingsProto device_settings_proto_;

  base::WeakPtrFactory<DeviceOffHoursController> weak_ptr_factory_{this};
};

}  // namespace off_hours
}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_OFF_HOURS_DEVICE_OFF_HOURS_CONTROLLER_H_
