// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/device_scheduled_update_checker.h"

#include <time.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor_impl.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/task_executor_with_retries.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/timezone_settings.h"

namespace policy {

namespace {

// Reason associated to acquire |ScopedWakeLock|.
constexpr char kWakeLockReason[] = "DeviceScheduledUpdateChecker";

// Task name used for parsing ScheduledTaskData.
constexpr char kTaskTimeFieldName[] = "update_check_time";

}  // namespace

// |cros_settings_subscription_| will be destroyed as part of this object
// guaranteeing to not run |OnScheduledUpdateCheckDataChanged| after its
// destruction. Therefore, it's safe to use "this" while adding this observer.
// Similarly, |start_update_check_timer_task_executor_| and
// |os_and_policies_update_checker_| will be destroyed as part of this object,
// so it's safe to use "this" with any callbacks.
DeviceScheduledUpdateChecker::DeviceScheduledUpdateChecker(
    ash::CrosSettings* cros_settings,
    ash::NetworkStateHandler* network_state_handler,
    std::unique_ptr<ScheduledTaskExecutor> update_check_executor)
    : cros_settings_(cros_settings),
      cros_settings_subscription_(cros_settings_->AddSettingsObserver(
          ash::kDeviceScheduledUpdateCheck,
          base::BindRepeating(
              &DeviceScheduledUpdateChecker::OnScheduledUpdateCheckDataChanged,
              base::Unretained(this)))),
      start_update_check_timer_task_executor_(
          update_checker_internal::kMaxStartUpdateCheckTimerRetryIterations,
          update_checker_internal::kStartUpdateCheckTimerRetryTime),
      os_and_policies_update_checker_(network_state_handler),
      update_check_executor_(std::move(update_check_executor)) {
  ash::system::TimezoneSettings::GetInstance()->AddObserver(this);
  // Check if policy already exists.
  OnScheduledUpdateCheckDataChanged();
}

DeviceScheduledUpdateChecker::~DeviceScheduledUpdateChecker() {
  ash::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void DeviceScheduledUpdateChecker::OnUpdateCheckTimerExpired() {
  // Following things needs to be done on every update check event. These will
  // be done serially to make state management easier -
  // - Download updates and refresh policies.
  // - Calculate and start the next update check timer.
  // For both these operations a wake lock should be held to ensure that the
  // update check is completed successfully and the device doesn't suspend while
  // calculating time ticks and setting the timer for the next update check. The
  // wake lock is held even when the above two operations are retried on
  // failure. This decision was made to simplify the design.
  //
  // The wake lock will be released in |OnUpdateCheckTimerStartResult| either
  // due to successful completion of step 2 above or failure and letting the
  // device suspend and reacquire the wake lock again in
  // |StartUpdateCheckTimer|.

  // If no policy exists, state should have been reset and this callback
  // shouldn't have fired.
  DCHECK(scheduled_update_check_data_);

  // |os_and_policies_update_checker_| will be destroyed as part of this object,
  // so it's safe to use "this" with any callbacks. This overrides any previous
  // update check calls. Since, the minimum update frequency is daily there is
  // very little chance of stepping on an existing update check that hasn't
  // finished for a day. Timeouts will ensure that an update check completes,
  // successfully or unsuccessfully, way before a day.
  os_and_policies_update_checker_.Start(
      base::BindOnce(
          &DeviceScheduledUpdateChecker::OnUpdateCheckCompletion,
          base::Unretained(this),
          ScopedWakeLock(device::mojom::WakeLockType::kPreventAppSuspension,
                         kWakeLockReason)),
      update_checker_internal::kOsAndPoliciesUpdateCheckHardTimeout);
}

void DeviceScheduledUpdateChecker::TimezoneChanged(
    const icu::TimeZone& time_zone) {
  // Anytime the time zone changes, the update check timer delay should be
  // recalculated and the timer should be started with updated values according
  // to the new time zone.
  // |scheduled_update_check_data_->next_scheduled_task_time_ticks| also needs
  // to be reset, as it would be incorrect in the context of a new time zone.
  // For this purpose, treat it as a new policy and call
  // |OnScheduledUpdateCheckDataChanged| instead of |MaybeStartUpdateCheckTimer|
  // directly.
  OnScheduledUpdateCheckDataChanged();
}

void DeviceScheduledUpdateChecker::OnScheduledUpdateCheckDataChanged() {
  // If the policy is removed or is not supported on the device, then reset all
  // state including any existing update checks.
  // The policy is not supported if device can not reliably schedule RTC wake
  // in a required range. The specific feature used is one that describes a
  // a known bug on some platforms, where setting rtc wake further than 24 hours
  // away crashes the device. Alternative ways to fix it are too risky, since
  // they may break a bigger proportion of the devices when pushed.
  const base::Value* value =
      cros_settings_->GetPref(ash::kDeviceScheduledUpdateCheck);
  if (!base::FeatureList::IsEnabled(::features::kSupportsRtcWakeOver24Hours) ||
      !value) {
    ResetState();
    return;
  }

  // Keep any old policy timers running if a new policy is ill-formed and can't
  // be used to set a new timer.
  std::optional<ScheduledTaskExecutor::ScheduledTaskData>
      scheduled_update_check_data =
          scheduled_task_util::ParseScheduledTask(*value, kTaskTimeFieldName);
  if (!scheduled_update_check_data) {
    LOG(ERROR) << "Failed to parse policy";
    return;
  }

  // Policy has been updated, calculate and set |update_check_timer_| again.
  scheduled_update_check_data_ = std::move(scheduled_update_check_data);
  MaybeStartUpdateCheckTimer(
      ScopedWakeLock(device::mojom::WakeLockType::kPreventAppSuspension,
                     kWakeLockReason),
      false /* is_retry */);
}

void DeviceScheduledUpdateChecker::StartUpdateCheckTimer(
    ScopedWakeLock scoped_wake_lock) {
  // The device shouldn't suspend while calculating time ticks and setting the
  // timer for the next update check. Otherwise the next update check timer will
  // be inaccurately scheduled. Hence, a wake lock must always be held for this
  // entire task. The wake lock could already be held at this
  // point due to |OnUpdateCheckTimerExpired|.

  // |update_check_executor_| will be destroyed as part of this object and is
  // guaranteed to not run callbacks after its destruction. Therefore, it's
  // safe to use base::Unretained(this) when starting the executor.
  update_check_executor_->Start(
      &scheduled_update_check_data_.value(),
      base::BindOnce(
          &DeviceScheduledUpdateChecker::OnUpdateCheckTimerStartResult,
          base::Unretained(this), std::move(scoped_wake_lock)),
      base::BindOnce(&DeviceScheduledUpdateChecker::OnUpdateCheckTimerExpired,
                     base::Unretained(this)));
}

void DeviceScheduledUpdateChecker::OnUpdateCheckTimerStartResult(
    ScopedWakeLock scoped_wake_lock,
    bool result) {
  // Schedule a retry if |update_check_timer_| failed to start.
  if (!result) {
    LOG(ERROR) << "Failed to start update check timer";
    MaybeStartUpdateCheckTimer(std::move(scoped_wake_lock),
                               true /* is_retry */);
  }
}

void DeviceScheduledUpdateChecker::OnStartUpdateCheckTimerRetryFailure() {
  // Retrying has a limit. In the unlikely scenario this is met, the next update
  // check can only happen when a new policy comes in or Chrome is restarted.
  // The wake lock acquired in |MaybeStartUpdateCheckTimer| will be released
  // because |task| was destroyed in |start_update_check_timer_executor_|.
  LOG(ERROR) << "Failed to start update check timer after all retries";
  ResetState();
}

void DeviceScheduledUpdateChecker::MaybeStartUpdateCheckTimer(
    ScopedWakeLock scoped_wake_lock,
    bool is_retry) {
  // No need to start the next update check timer if the policy has been
  // removed. This can happen if an update check was ongoing, a new policy
  // came in but failed to start the timer which reset all state in
  // |OnStartUpdateCheckTimerRetryFailure|.
  if (!scheduled_update_check_data_)
    return;

  // Only start the timer if an existing update check is not running because if
  // it is, it will start the timer in |OnUpdateCheckCompletion|.
  if (os_and_policies_update_checker_.IsRunning())
    return;

  // Safe to use |this| as |start_update_check_timer_task_executor_| is a
  // member of |this|. Wake lock needs to be held to calculate next update check
  // timer accurately without the device suspending mid-calculation.
  if (is_retry) {
    start_update_check_timer_task_executor_.ScheduleRetry(
        base::BindOnce(&DeviceScheduledUpdateChecker::StartUpdateCheckTimer,
                       base::Unretained(this), std::move(scoped_wake_lock)));
  } else {
    start_update_check_timer_task_executor_.Start(
        base::BindOnce(&DeviceScheduledUpdateChecker::StartUpdateCheckTimer,
                       base::Unretained(this), std::move(scoped_wake_lock)),
        base::BindOnce(
            &DeviceScheduledUpdateChecker::OnStartUpdateCheckTimerRetryFailure,
            base::Unretained(this)));
  }
}

void DeviceScheduledUpdateChecker::OnUpdateCheckCompletion(
    ScopedWakeLock scoped_wake_lock,
    bool result) {
  // Start the next update check timer irrespective of the current update
  // check succeeding or not.
  LOG_IF(ERROR, !result) << "Update check failed";
  MaybeStartUpdateCheckTimer(std::move(scoped_wake_lock), false /* is_retry */);
}

void DeviceScheduledUpdateChecker::ResetState() {
  update_check_executor_->Reset();
  scheduled_update_check_data_ = std::nullopt;
  os_and_policies_update_checker_.Stop();
  start_update_check_timer_task_executor_.Stop();
}

}  // namespace policy
