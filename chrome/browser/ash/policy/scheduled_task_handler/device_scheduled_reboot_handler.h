// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_DEVICE_SCHEDULED_REBOOT_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_DEVICE_SCHEDULED_REBOOT_HANDLER_H_

#include <memory>

#include "ash/components/settings/timezone_settings.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scoped_wake_lock.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "services/device/public/mojom/wake_lock.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {

// This class listens for changes in the scheduled reboot policy and then
// manages recurring reboots based on the policy. Reboots are only applied if
// the device is in the kiosk mode.
class DeviceScheduledRebootHandler
    : public ash::system::TimezoneSettings::Observer {
 public:
  explicit DeviceScheduledRebootHandler(
      ash::CrosSettings* cros_settings,
      std::unique_ptr<ScheduledTaskExecutor> scheduled_task_executor);
  DeviceScheduledRebootHandler(const DeviceScheduledRebootHandler&) = delete;
  DeviceScheduledRebootHandler& operator=(const DeviceScheduledRebootHandler&) =
      delete;
  ~DeviceScheduledRebootHandler() override;

  // TODO(https://crbug.com/1228718): Move Timezone observation to
  // ScheduledTaskExecutor. ash::system::TimezoneSettings::Observer
  // implementation.
  void TimezoneChanged(const icu::TimeZone& time_zone) override;

  // The tag associated to register |scheduled_task_executor_|.
  static constexpr char kRebootTimerTag[] = "DeviceScheduledRebootHandler";

 protected:
  // Called when scheduled timer fires. Triggers a reboot and
  // schedules the next reboot based on |scheduled_reboot_data_|.
  virtual void OnRebootTimerExpired();

 private:
  // Callback triggered when scheduled reboot setting has changed.
  void OnScheduledRebootDataChanged();

  // Calls |scheduled_task_executor_| to start the timer. Requires
  // |scheduled_update_check_data_| to be set.
  void StartRebootTimer();

  // Called upon starting reboot timer. Indicates whether or not the
  // timer was started successfully.
  void OnRebootTimerStartResult(ScopedWakeLock scoped_wake_lock, bool result);

  // Reset all state and cancel all pending tasks
  void ResetState();

  // Used to retrieve Chrome OS settings. Not owned.
  ash::CrosSettings* const cros_settings_;

  // Subscription for callback when settings change.
  base::CallbackListSubscription cros_settings_subscription_;

  // Currently active scheduled reboot policy.
  absl::optional<ScheduledTaskExecutor::ScheduledTaskData>
      scheduled_reboot_data_;

  // Timer that is scheduled to check for updates.
  std::unique_ptr<ScheduledTaskExecutor> scheduled_task_executor_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_DEVICE_SCHEDULED_REBOOT_HANDLER_H_
