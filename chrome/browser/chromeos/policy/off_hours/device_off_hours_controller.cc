// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/off_hours/device_off_hours_controller.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/time/default_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/chromeos/policy/off_hours/off_hours_proto_parser.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/policy/weekly_time/time_utils.h"
#include "components/prefs/pref_value_map.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace em = enterprise_management;

namespace policy {
namespace off_hours {

DeviceOffHoursController::DeviceOffHoursController()
    : timer_(std::make_unique<base::OneShotTimer>()),
      clock_(base::DefaultClock::GetInstance()) {
  auto* system_clock_client = chromeos::SystemClockClient::Get();
  if (system_clock_client) {
    system_clock_client->AddObserver(this);
    system_clock_client->WaitForServiceToBeAvailable(
        base::Bind(&DeviceOffHoursController::SystemClockInitiallyAvailable,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::Get()->AddObserver(this);
}

DeviceOffHoursController::~DeviceOffHoursController() {
  if (chromeos::SystemClockClient::Get())
    chromeos::SystemClockClient::Get()->RemoveObserver(this);

  if (chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void DeviceOffHoursController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceOffHoursController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceOffHoursController::SetClockForTesting(
    base::Clock* clock,
    const base::TickClock* timer_clock) {
  clock_ = clock;
  timer_ = std::make_unique<base::OneShotTimer>(timer_clock);
}

bool DeviceOffHoursController::IsCurrentSessionAllowedOnlyForOffHours() const {
  if (!is_off_hours_mode())
    return false;

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  const user_manager::UserList& logged_in_users =
      user_manager->GetLoggedInUsers();
  user_manager::UserList users_to_check;
  for (auto* user : logged_in_users) {
    if (user->GetType() == user_manager::USER_TYPE_REGULAR ||
        user->GetType() == user_manager::USER_TYPE_GUEST ||
        user->GetType() == user_manager::USER_TYPE_SUPERVISED ||
        user->GetType() == user_manager::USER_TYPE_CHILD) {
      users_to_check.push_back(user);
    }
  }

  if (users_to_check.empty())
    return false;

  // If at least one logged in user won't be allowed after OffHours,
  // the session will be terminated.
  return !chromeos::chrome_user_manager_util::AreAllUsersAllowed(
      users_to_check, device_settings_proto_);
}

void DeviceOffHoursController::UpdateOffHoursPolicy(
    const em::ChromeDeviceSettingsProto& device_settings_proto) {
  device_settings_proto_ = device_settings_proto;
  std::vector<WeeklyTimeInterval> off_hours_intervals;
  if (device_settings_proto.has_device_off_hours()) {
    const em::DeviceOffHoursProto& container(
        device_settings_proto.device_off_hours());
    base::Optional<std::string> timezone = ExtractTimezoneFromProto(container);
    if (timezone) {
      off_hours_intervals = weekly_time_utils::ConvertIntervalsToGmt(
          ExtractWeeklyTimeIntervalsFromProto(container, *timezone, clock_));
    }
  }
  off_hours_intervals_.swap(off_hours_intervals);
  UpdateOffHoursMode();
}

void DeviceOffHoursController::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  // Triggered when device wakes up. "OffHours" state could be changed during
  // sleep mode and should be updated after that.
  UpdateOffHoursMode();
}

void DeviceOffHoursController::NotifyOffHoursEndTimeChanged() const {
  VLOG(1) << "OffHours end time is changed to " << off_hours_end_time_;
  for (auto& observer : observers_)
    observer.OnOffHoursEndTimeChanged();
}

void DeviceOffHoursController::OffHoursModeIsChanged() const {
  VLOG(1) << "OffHours mode is changed to " << off_hours_mode_;
  chromeos::DeviceSettingsService::Get()->Load();
}

void DeviceOffHoursController::UpdateOffHoursMode() {
  // Assume that time is network synchronized if response from dbus call is not
  // arrived.
  bool is_time_network_synchronized = network_synchronized_.value_or(true);
  if (off_hours_intervals_.empty() || !is_time_network_synchronized) {
    if (!is_time_network_synchronized) {
      VLOG(1) << "The system time isn't network synchronized. OffHours mode is "
                 "unavailable.";
    }
    StopOffHoursTimer();
    SetOffHoursMode(false);
    return;
  }
  WeeklyTime current_time = WeeklyTime::GetCurrentGmtWeeklyTime(clock_);
  for (const auto& interval : off_hours_intervals_) {
    if (interval.Contains(current_time)) {
      base::TimeDelta remaining_off_hours_duration =
          current_time.GetDurationTo(interval.end());
      SetOffHoursEndTime(base::TimeTicks::Now() + remaining_off_hours_duration);
      StartOffHoursTimer(remaining_off_hours_duration);
      SetOffHoursMode(true);
      return;
    }
  }
  StartOffHoursTimer(weekly_time_utils::GetDeltaTillNextTimeInterval(
      current_time, off_hours_intervals_));
  SetOffHoursMode(false);
}

void DeviceOffHoursController::SetOffHoursEndTime(
    base::TimeTicks off_hours_end_time) {
  if (off_hours_end_time == off_hours_end_time_)
    return;
  off_hours_end_time_ = off_hours_end_time;
  NotifyOffHoursEndTimeChanged();
}

void DeviceOffHoursController::SetOffHoursMode(bool off_hours_enabled) {
  if (off_hours_mode_ == off_hours_enabled)
    return;
  off_hours_mode_ = off_hours_enabled;
  DVLOG(1) << "OffHours mode: " << off_hours_mode_;
  if (!off_hours_mode_)
    SetOffHoursEndTime(base::TimeTicks());
  OffHoursModeIsChanged();
}

void DeviceOffHoursController::StartOffHoursTimer(base::TimeDelta delay) {
  DCHECK_GT(delay, base::TimeDelta());
  DVLOG(1) << "OffHours mode timer starts for " << delay;
  timer_->Start(FROM_HERE, delay,
                base::Bind(&DeviceOffHoursController::UpdateOffHoursMode,
                           weak_ptr_factory_.GetWeakPtr()));
}

void DeviceOffHoursController::StopOffHoursTimer() {
  timer_->Stop();
}

void DeviceOffHoursController::SystemClockUpdated() {
  // Triggered when the device time is changed. When it happens the "OffHours"
  // mode could be changed too, because "OffHours" mode directly depends on the
  // current device time. Ask SystemClockClient to update information about the
  // system time synchronization with the network time asynchronously.
  // Information will be received by NetworkSynchronizationUpdated method.
  chromeos::SystemClockClient::Get()->GetLastSyncInfo(
      base::Bind(&DeviceOffHoursController::NetworkSynchronizationUpdated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceOffHoursController::SystemClockInitiallyAvailable(
    bool service_is_available) {
  if (!service_is_available)
    return;
  chromeos::SystemClockClient::Get()->GetLastSyncInfo(
      base::Bind(&DeviceOffHoursController::NetworkSynchronizationUpdated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceOffHoursController::NetworkSynchronizationUpdated(
    bool network_synchronized) {
  // Triggered when information about the system time synchronization with
  // network is received.
  network_synchronized_ = network_synchronized;
  UpdateOffHoursMode();
}

}  // namespace off_hours
}  // namespace policy
