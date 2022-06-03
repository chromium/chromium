// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/device_scheduled_reboot_handler.h"

#include <time.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/components/settings/cros_settings_names.h"
#include "ash/components/settings/timezone_settings.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scoped_wake_lock.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

namespace {

// Description associated with requesting restart
constexpr char kRebootDescription[] = "device scheduled reboot";

// Reason associated to acquire |ScopedWakeLock|.
constexpr char kWakeLockReason[] = "DeviceScheduledRebootHandler";

// Task name used for parsing ScheduledTaskData.
constexpr char kTaskTimeFieldName[] = "reboot_time";

}  // namespace

constexpr char DeviceScheduledRebootHandler::kRebootTimerTag[];

DeviceScheduledRebootHandler::DeviceScheduledRebootHandler(
    ash::CrosSettings* cros_settings,
    std::unique_ptr<ScheduledTaskExecutor> scheduled_task_executor)
    : cros_settings_(cros_settings),
      cros_settings_subscription_(cros_settings_->AddSettingsObserver(
          ash::kDeviceScheduledReboot,
          base::BindRepeating(
              &DeviceScheduledRebootHandler::OnScheduledRebootDataChanged,
              base::Unretained(this)))),
      scheduled_task_executor_(std::move(scheduled_task_executor)) {
  ash::system::TimezoneSettings::GetInstance()->AddObserver(this);
  // Check if policy already exists.
  OnScheduledRebootDataChanged();
}

DeviceScheduledRebootHandler::~DeviceScheduledRebootHandler() {
  ash::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void DeviceScheduledRebootHandler::OnRebootTimerExpired() {
  // If no policy exists, state should have been reset and this callback
  // shouldn't have fired.
  DCHECK(scheduled_reboot_data_);

  // Only request restart if the device is in the kiosk mode. Once the device
  // has rebooted, the handler will be created again and the reboot will be
  // rescheduled.
  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    ash::PowerManagerClient::Get()->RequestRestart(
        power_manager::REQUEST_RESTART_OTHER, kRebootDescription);
    return;
  }
  // If the device is not in the kiosk mode, start the timer again and try for
  // the reboot next time on schedule.
  StartRebootTimer();
}

void DeviceScheduledRebootHandler::TimezoneChanged(
    const icu::TimeZone& time_zone) {
  // Anytime the time zone changes,
  // |scheduled_reboot_data_->next_scheduled_task_time_ticks| needs to be reset,
  // as it would be incorrect in the context of a new time zone. For this
  // purpose, treat it as a new policy and call |OnScheduledRebootDataChanged|
  // instead of |StartRebootTimer| directly.
  OnScheduledRebootDataChanged();
}

void DeviceScheduledRebootHandler::OnScheduledRebootDataChanged() {
  // If the policy is removed then reset all state including any existing
  // scheduled reboots.
  const base::Value* value =
      cros_settings_->GetPref(ash::kDeviceScheduledReboot);
  if (!value) {
    ResetState();
    return;
  }

  // Keep any old policy timers running if a new policy is ill-formed and can't
  // be used to set a new timer.
  absl::optional<ScheduledTaskExecutor::ScheduledTaskData>
      scheduled_reboot_data =
          scheduled_task_util::ParseScheduledTask(*value, kTaskTimeFieldName);
  if (!scheduled_reboot_data) {
    LOG(ERROR) << "Failed to parse policy";
    return;
  }

  // Policy has been updated, calculate and set the timer again.
  scheduled_reboot_data_ = std::move(scheduled_reboot_data);
  StartRebootTimer();
}

void DeviceScheduledRebootHandler::StartRebootTimer() {
  // The device shouldn't suspend while calculating time ticks and setting the
  // timer for the next reboot. Otherwise the next reboot timer will
  // be inaccurately scheduled. Hence, a wake lock must always be held for this
  // entire task.
  scheduled_task_executor_->Start(
      &scheduled_reboot_data_.value(),
      base::BindOnce(
          &DeviceScheduledRebootHandler::OnRebootTimerStartResult,
          base::Unretained(this),
          ScopedWakeLock(device::mojom::WakeLockType::kPreventAppSuspension,
                         kWakeLockReason)),
      base::BindOnce(&DeviceScheduledRebootHandler::OnRebootTimerExpired,
                     base::Unretained(this)));
}

void DeviceScheduledRebootHandler::OnRebootTimerStartResult(
    ScopedWakeLock scoped_wake_lock,
    bool result) {
  // If reboot timer failed to start, reset state. The reboot will be scheduled
  // again when the new policy comes or Chrome is restarted.
  if (!result) {
    LOG(ERROR) << "Failed to start reboot timer";
    ResetState();
  }
}

void DeviceScheduledRebootHandler::ResetState() {
  scheduled_task_executor_->Reset();
  scheduled_reboot_data_ = absl::nullopt;
}

}  // namespace policy
