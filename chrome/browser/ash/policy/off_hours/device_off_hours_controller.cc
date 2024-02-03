// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/off_hours/device_off_hours_controller.h"

#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/default_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/policy/off_hours/off_hours_proto_parser.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/policy/weekly_time/time_utils.h"
#include "components/prefs/pref_value_map.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace policy::off_hours {

namespace em = ::enterprise_management;

DeviceOffHoursController::DeviceOffHoursController()
    : timer_(std::make_unique<base::WallClockTimer>()),
      clock_(base::DefaultClock::GetInstance()) {
  auto* system_clock_client = ash::SystemClockClient::Get();
  if (system_clock_client) {
    system_clock_client->AddObserver(this);
    system_clock_client->WaitForServiceToBeAvailable(
        base::BindOnce(&DeviceOffHoursController::SystemClockInitiallyAvailable,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

DeviceOffHoursController::~DeviceOffHoursController() {
  if (ash::SystemClockClient::Get())
    ash::SystemClockClient::Get()->RemoveObserver(this);
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
  timer_ = std::make_unique<base::WallClockTimer>(clock, timer_clock);
}

bool DeviceOffHoursController::IsCurrentSessionAllowedOnlyForOffHours() const {
  if (!is_off_hours_mode())
    return false;

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  const user_manager::UserList& logged_in_users =
      user_manager->GetLoggedInUsers();
  user_manager::UserList users_to_check;
  for (user_manager::User* user : logged_in_users) {
    if (user->GetType() == user_manager::UserType::kRegular ||
        user->GetType() == user_manager::UserType::kGuest ||
        user->GetType() == user_manager::UserType::kChild) {
      users_to_check.push_back(user);
    }
  }

  if (users_to_check.empty())
    return false;

  // If at least one logged in user won't be allowed after OffHours,
  // the session will be terminated.
  return !ash::chrome_user_manager_util::AreAllUsersAllowed(
      users_to_check, device_settings_proto_);
}

void DeviceOffHoursController::UpdateOffHoursPolicy(
    const em::ChromeDeviceSettingsProto& device_settings_proto) {
  device_settings_proto_ = device_settings_proto;
  std::vector<WeeklyTimeInterval> off_hours_intervals;
  if (device_settings_proto.has_device_off_hours()) {
    const em::DeviceOffHoursProto& container(
        device_settings_proto.device_off_hours());
    std::optional<std::string> timezone = ExtractTimezoneFromProto(container);
    if (timezone) {
      off_hours_intervals = weekly_time_utils::ConvertIntervalsToGmt(
          ExtractWeeklyTimeIntervalsFromProto(container, *timezone, clock_));
    }
  }
  off_hours_intervals_.swap(off_hours_intervals);
  UpdateOffHoursMode();
}

void DeviceOffHoursController::NotifyOffHoursEndTimeChanged() const {
  VLOG(1) << "OffHours end time is changed to " << off_hours_end_time_;
  for (auto& observer : observers_)
    observer.OnOffHoursEndTimeChanged();
}

void DeviceOffHoursController::OffHoursModeIsChanged() const {
  VLOG(1) << "OffHours mode is changed to " << off_hours_mode_;
  ash::DeviceSettingsService::Get()->Load();
}

void DeviceOffHoursController::UpdateOffHoursMode() {
  if (off_hours_intervals_.empty() || !is_clock_network_synchronized_) {
    if (!is_clock_network_synchronized_) {
      VLOG(1) << "The system clock isn't network synchronized. OffHours mode "
                 "is unavailable.";
    }
    SetOffHoursEndTime(base::Time{});
    StopOffHoursTimer();
    SetOffHoursMode(false);
    return;
  }

  namespace wtu = weekly_time_utils;
  const base::Time now = clock_->Now();
  const bool in_interval = wtu::Contains(now, off_hours_intervals_);
  const std::optional<base::Time> update_time =
      wtu::GetNextEventTime(now, off_hours_intervals_);

  // weekly off_hours_intervals_ is not empty -> update_time has a value
  DCHECK(update_time);

  SetOffHoursEndTime(in_interval ? update_time.value() : base::Time{});
  StartOffHoursTimer(update_time.value());
  SetOffHoursMode(in_interval);
}

void DeviceOffHoursController::SetOffHoursEndTime(
    base::Time off_hours_end_time) {
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
    SetOffHoursEndTime(base::Time());
  OffHoursModeIsChanged();
}

void DeviceOffHoursController::StartOffHoursTimer(base::Time update_time) {
  DVLOG(1) << "OffHours mode timer starts with run time " << update_time;
  timer_->Start(FROM_HERE, update_time,
                base::BindOnce(&DeviceOffHoursController::UpdateOffHoursMode,
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
  ash::SystemClockClient::Get()->GetLastSyncInfo(
      base::BindOnce(&DeviceOffHoursController::NetworkSynchronizationUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceOffHoursController::SystemClockInitiallyAvailable(
    bool service_is_available) {
  if (!service_is_available)
    return;
  ash::SystemClockClient::Get()->GetLastSyncInfo(
      base::BindOnce(&DeviceOffHoursController::NetworkSynchronizationUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceOffHoursController::NetworkSynchronizationUpdated(
    bool network_synchronized) {
  // Triggered when information about the system time synchronization with
  // network is received.
  is_clock_network_synchronized_ = network_synchronized;
  UpdateOffHoursMode();
}

}  // namespace policy::off_hours
