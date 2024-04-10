// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_DEVICE_SCHEDULED_UPDATE_CHECKER_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_DEVICE_SCHEDULED_UPDATE_CHECKER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/os_and_policies_update_checker.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scoped_wake_lock.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/task_executor_with_retries.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "services/device/public/mojom/wake_lock.mojom-forward.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {

// This class listens for changes in the scheduled update check policy and then
// manages recurring update checks based on the policy.
class DeviceScheduledUpdateChecker
    : public ash::system::TimezoneSettings::Observer {
 public:
  DeviceScheduledUpdateChecker(
      ash::CrosSettings* cros_settings,
      ash::NetworkStateHandler* network_state_handler,
      std::unique_ptr<ScheduledTaskExecutor> update_check_executor);

  DeviceScheduledUpdateChecker(const DeviceScheduledUpdateChecker&) = delete;
  DeviceScheduledUpdateChecker& operator=(const DeviceScheduledUpdateChecker&) =
      delete;

  ~DeviceScheduledUpdateChecker() override;

  // ash::system::TimezoneSettings::Observer implementation.
  void TimezoneChanged(const icu::TimeZone& time_zone) override;

 protected:
  // Called when |update_check_timer_| fires. Triggers an update check and
  // schedules the next update check based on |scheduled_update_check_data_|.
  virtual void OnUpdateCheckTimerExpired();

  // Called when |os_and_policies_update_checker_| has finished successfully or
  // unsuccessfully after retrying.
  virtual void OnUpdateCheckCompletion(ScopedWakeLock scoped_wake_lock,
                                       bool result);

 private:
  // Callback triggered when scheduled update check setting has changed.
  void OnScheduledUpdateCheckDataChanged();

  // Must only be run via |start_update_check_timer_task_executor_|. Sets
  // |update_check_timer_| based on |scheduled_update_check_data_|. If the
  // |update_check_timer_| can't be started due to an error in
  // |CalculateNextUpdateCheckTimerDelay| then reschedules itself via
  // |start_update_check_timer_task_executor_|. Requires
  // |scheduled_update_check_data_| to be set.
  void StartUpdateCheckTimer(ScopedWakeLock scoped_wake_lock);

  // Called upon starting |update_check_timer_|. Indicates whether or not the
  // timer was started successfully.
  void OnUpdateCheckTimerStartResult(ScopedWakeLock scoped_wake_lock,
                                     bool result);

  // Called when |start_update_check_timer_task_executor_|'s retry limit has
  // been reached.
  void OnStartUpdateCheckTimerRetryFailure();

  // Starts or retries |StartUpdateCheckTimer| via
  // |start_update_check_timer_task_executor_| based on |is_retry|.
  void MaybeStartUpdateCheckTimer(ScopedWakeLock scoped_wake_lock,
                                  bool is_retry);

  // Reset all state and cancel all pending tasks
  void ResetState();

  // Used to retrieve Chrome OS settings. Not owned.
  const raw_ptr<ash::CrosSettings> cros_settings_;

  // Subscription for callback when settings change.
  base::CallbackListSubscription cros_settings_subscription_;

  // Currently active scheduled update check policy.
  std::optional<ScheduledTaskExecutor::ScheduledTaskData>
      scheduled_update_check_data_;

  // Used to run and retry |StartUpdateCheckTimer| if it fails.
  TaskExecutorWithRetries start_update_check_timer_task_executor_;

  // Used to initiate update checks when |update_check_timer_| fires.
  OsAndPoliciesUpdateChecker os_and_policies_update_checker_;

  // Timer that is scheduled to check for updates.
  std::unique_ptr<ScheduledTaskExecutor> update_check_executor_;
};

namespace update_checker_internal {

// The tag associated to register |update_check_executor_|.
constexpr char kUpdateCheckTimerTag[] = "DeviceScheduledUpdateChecker";

// The timeout after which an OS and policies update is aborted.
constexpr base::TimeDelta kOsAndPoliciesUpdateCheckHardTimeout =
    base::Minutes(40);

// The maximum iterations allowed to start an update check timer if the
// operation fails.
constexpr int kMaxStartUpdateCheckTimerRetryIterations = 5;

// Time to call |StartUpdateCheckTimer| again in case it failed.
constexpr base::TimeDelta kStartUpdateCheckTimerRetryTime = base::Minutes(1);

}  // namespace update_checker_internal

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_DEVICE_SCHEDULED_UPDATE_CHECKER_H_
