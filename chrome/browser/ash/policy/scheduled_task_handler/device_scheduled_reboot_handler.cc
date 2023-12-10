// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/device_scheduled_reboot_handler.h"

#include <time.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scoped_wake_lock.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/power_manager/dbus-constants.h"

namespace policy {

namespace {

// Description associated with requesting restart on timer expired.
constexpr char kRebootDescriptionOnTimerExpired[] = "device scheduled reboot";

// Description associated with requesting restart on button click.
constexpr char kRebootDescriptionOnButtonClicked[] = "reboot button clicked";

// Reason associated to acquire |ScopedWakeLock|.
constexpr char kWakeLockReason[] = "DeviceScheduledRebootHandler";

// Task name used for parsing ScheduledTaskData.
constexpr char kTaskTimeFieldName[] = "reboot_time";

base::Time GetBootTime() {
  return base::Time::Now() - base::SysInfo::Uptime();
}

}  // namespace

constexpr char DeviceScheduledRebootHandler::kRebootTimerTag[];

DeviceScheduledRebootHandler::DeviceScheduledRebootHandler(
    ash::CrosSettings* cros_settings,
    std::unique_ptr<ScheduledTaskExecutor> scheduled_task_executor,
    RebootNotificationsScheduler* notifications_scheduler)
    : DeviceScheduledRebootHandler(cros_settings,
                                   std::move(scheduled_task_executor),
                                   notifications_scheduler,
                                   base::BindRepeating(GetBootTime)) {}

DeviceScheduledRebootHandler::DeviceScheduledRebootHandler(
    ash::CrosSettings* cros_settings,
    std::unique_ptr<ScheduledTaskExecutor> scheduled_task_executor,
    RebootNotificationsScheduler* notifications_scheduler,
    GetBootTimeCallback get_boot_time_callback)
    : cros_settings_(cros_settings),
      cros_settings_subscription_(cros_settings_->AddSettingsObserver(
          ash::kDeviceScheduledReboot,
          base::BindRepeating(
              &DeviceScheduledRebootHandler::OnScheduledRebootDataChanged,
              base::Unretained(this)))),
      scheduled_task_executor_(std::move(scheduled_task_executor)),
      notifications_scheduler_(notifications_scheduler),
      get_boot_time_callback_(std::move(get_boot_time_callback)) {
  DCHECK(get_boot_time_callback_);

  ash::system::TimezoneSettings::GetInstance()->AddObserver(this);
  auto* power_manager_client = chromeos::PowerManagerClient::Get();
  if (power_manager_client) {
    observation_.Observe(power_manager_client);
  }
}

DeviceScheduledRebootHandler::~DeviceScheduledRebootHandler() {
  observation_.Reset();
  ash::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
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

void DeviceScheduledRebootHandler::PowerManagerBecameAvailable(bool available) {
  if (!available) {
    LOG(ERROR) << "Power manager service is not available. Not possible to "
                  "schedule reboot.";
    ResetState();
    return;
  }
  // Check if policy already exists.
  OnScheduledRebootDataChanged();
}

void DeviceScheduledRebootHandler::SetRebootDelayForTest(
    const base::TimeDelta& reboot_delay) {
  reboot_delay_for_testing_ = reboot_delay;
}

std::optional<ScheduledTaskExecutor::ScheduledTaskData>
DeviceScheduledRebootHandler::GetScheduledRebootDataForTest() const {
  return scheduled_reboot_data_;
}

bool DeviceScheduledRebootHandler::IsRebootSkippedForTest() const {
  return skip_reboot_;
}

void DeviceScheduledRebootHandler::OnRebootTimerExpired() {
  // If no policy exists, state should have been reset and this callback
  // shouldn't have fired.
  DCHECK(scheduled_reboot_data_);

  // Always request restart if the device is in the kiosk mode or on the sign-in
  // screen. Once the device has rebooted, the handler will be created again and
  // the reboot will be rescheduled.
  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    RebootDevice(kRebootDescriptionOnTimerExpired);
    return;
  }

  // If the device is on the sign-in screen, skip reboot only if the grace
  // period is applied.
  if (!skip_reboot_ && !user_manager::UserManager::Get()->IsUserLoggedIn() &&
      base::FeatureList::IsEnabled(
          ash::features::kDeviceForceScheduledReboot)) {
    RebootDevice(kRebootDescriptionOnTimerExpired);
    return;
  }

  // If the device is not in the kiosk mode or on the sign-in screen, check if
  // the |kDeviceForceScheduledReboot| feature flag is enabled and if we should
  // not skip reboot due to grace period applied.
  if (!skip_reboot_ && base::FeatureList::IsEnabled(
                           ash::features::kDeviceForceScheduledReboot)) {
    // Schedule post reboot notification for the user in session.
    notifications_scheduler_->SchedulePostRebootNotification();
    RebootDevice(kRebootDescriptionOnTimerExpired);
    return;
  }

  // Start the timer again and try for the reboot next time on schedule.
  skip_reboot_ = false;
  StartRebootTimer();
}

void DeviceScheduledRebootHandler::OnRebootButtonClicked() {
  RebootDevice(kRebootDescriptionOnButtonClicked);
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
  std::optional<ScheduledTaskExecutor::ScheduledTaskData>
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
  // Always add delay to scheduled task time.
  scheduled_task_executor_->Start(
      &scheduled_reboot_data_.value(),
      base::BindOnce(
          &DeviceScheduledRebootHandler::OnRebootTimerStartResult,
          base::Unretained(this),
          ScopedWakeLock(device::mojom::WakeLockType::kPreventAppSuspension,
                         kWakeLockReason)),
      base::BindOnce(&DeviceScheduledRebootHandler::OnRebootTimerExpired,
                     base::Unretained(this)),
      GetExternalDelay());
}

void DeviceScheduledRebootHandler::OnRebootTimerStartResult(
    ScopedWakeLock scoped_wake_lock,
    bool result) {
  // If reboot timer failed to start, reset notifications if scheduled and
  // |scheduled_reboot_data_|. The reboot will be scheduled again when the new
  // policy comes or Chrome is restarted.
  if (!result) {
    LOG(ERROR) << "Failed to start reboot timer";
    notifications_scheduler_->CancelRebootNotifications(
        RebootNotificationsScheduler::Requester::kScheduledRebootPolicy);
    skip_reboot_ = false;
    scheduled_reboot_data_ = std::nullopt;
    return;
  }

  skip_reboot_ = scheduled_task_util::ShouldSkipRebootDueToGracePeriod(
      get_boot_time_callback_.Run(),
      scheduled_task_executor_->GetScheduledTaskTime());

  // If the flag is enabled, schedule reboot notification and dialog.
  if (base::FeatureList::IsEnabled(
          ash::features::kDeviceForceScheduledReboot)) {
    if (!skip_reboot_) {
      notifications_scheduler_->SchedulePendingRebootNotifications(
          base::BindOnce(&DeviceScheduledRebootHandler::OnRebootButtonClicked,
                         base::Unretained(this)),
          scheduled_task_executor_->GetScheduledTaskTime(),
          RebootNotificationsScheduler::Requester::kScheduledRebootPolicy);
    }
  }
}

void DeviceScheduledRebootHandler::ResetState() {
  notifications_scheduler_->CancelRebootNotifications(
      RebootNotificationsScheduler::Requester::kScheduledRebootPolicy);
  scheduled_task_executor_->Reset();
  skip_reboot_ = false;
  scheduled_reboot_data_ = std::nullopt;
}

const base::TimeDelta DeviceScheduledRebootHandler::GetExternalDelay() const {
  return reboot_delay_for_testing_.has_value()
             ? reboot_delay_for_testing_.value()
             : base::RandTimeDeltaUpTo(
                   ash::features::kDeviceForceScheduledRebootMaxDelay.Get());
}

void DeviceScheduledRebootHandler::RebootDevice(
    const std::string& reboot_description) const {
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_SCHEDULED_REBOOT_POLICY,
      reboot_description);
}

}  // namespace policy
