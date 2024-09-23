// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_DEVICE_SCHEDULED_REBOOT_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_DEVICE_SCHEDULED_REBOOT_HANDLER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/reboot_notifications_scheduler.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scoped_wake_lock.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "services/device/public/mojom/wake_lock.mojom-forward.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {

// This class listens for changes in the scheduled reboot policy and then
// manages recurring reboots based on the policy.
class DeviceScheduledRebootHandler
    : public ash::system::TimezoneSettings::Observer,
      chromeos::PowerManagerClient::Observer {
 public:
  DeviceScheduledRebootHandler(
      ash::CrosSettings* cros_settings,
      std::unique_ptr<ScheduledTaskExecutor> scheduled_task_executor,
      RebootNotificationsScheduler* notifications_scheduler);
  DeviceScheduledRebootHandler(const DeviceScheduledRebootHandler&) = delete;
  DeviceScheduledRebootHandler& operator=(const DeviceScheduledRebootHandler&) =
      delete;
  ~DeviceScheduledRebootHandler() override;

  // TODO(https://crbug.com/1228718): Move Timezone observation to
  // ScheduledTaskExecutor. ash::system::TimezoneSettings::Observer
  // implementation.
  void TimezoneChanged(const icu::TimeZone& time_zone) override;

  // Called when the power manager service becomes available. Reboot timer can
  // only be started after this moment.
  // chromeos::PowerManagerClient::Observer overrides:
  void PowerManagerBecameAvailable(bool available) override;

  // The tag associated to register |scheduled_task_executor_|.
  static constexpr char kRebootTimerTag[] = "DeviceScheduledRebootHandler";

  // Sets reboot delay for testing.
  void SetRebootDelayForTest(const base::TimeDelta& reboot_delay);

  // Returns value of |scheduled_reboot_data_|.
  std::optional<ScheduledTaskExecutor::ScheduledTaskData>
  GetScheduledRebootDataForTest() const;

  // Returns value of |skip_reboot_|.
  bool IsRebootSkippedForTest() const;

 protected:
  using GetBootTimeCallback = base::RepeatingCallback<base::Time()>;

  // Extended constructor for testing purposes. `cros_settings` and
  // `notifications_scheduler` must outlive the handler.
  DeviceScheduledRebootHandler(
      ash::CrosSettings* cros_settings,
      std::unique_ptr<ScheduledTaskExecutor> scheduled_task_executor,
      RebootNotificationsScheduler* notifications_scheduler,
      GetBootTimeCallback get_boot_time_callback);

  // Called when scheduled timer fires. Triggers a reboot and
  // schedules the next reboot based on |scheduled_reboot_data_|.
  virtual void OnRebootTimerExpired();

  // Called on button click on the reboot notification or dialog. Executes
  // reboot instantly.
  virtual void OnRebootButtonClicked();

  // Callback triggered when scheduled reboot setting has changed.
  virtual void OnScheduledRebootDataChanged();

 private:
  // Calls |scheduled_task_executor_| to start the timer. Requires
  // |scheduled_update_check_data_| to be set.
  void StartRebootTimer();

  // Called upon starting reboot timer. Indicates whether or not the
  // timer was started successfully.
  void OnRebootTimerStartResult(ScopedWakeLock scoped_wake_lock, bool result);

  // Reset all state and cancel all pending tasks
  void ResetState();

  // Returns random delay between 0 and maximum reboot delay set by
  // ash::features::kDeviceForceScheduledRebootMaxDelay feature or
  // |reboot_delay_for_testing_| if set.
  const base::TimeDelta GetExternalDelay() const;

  void RebootDevice(const std::string& reboot_description) const;

  // Used to retrieve Chrome OS settings. Not owned.
  const raw_ptr<ash::CrosSettings> cros_settings_;

  // Subscription for callback when settings change.
  base::CallbackListSubscription cros_settings_subscription_;

  // Currently active scheduled reboot policy.
  std::optional<ScheduledTaskExecutor::ScheduledTaskData>
      scheduled_reboot_data_;

  // Timer that is scheduled to check for updates.
  std::unique_ptr<ScheduledTaskExecutor> scheduled_task_executor_;

  // Delay added to scheduled reboot time, used for testing.
  std::optional<base::TimeDelta> reboot_delay_for_testing_;

  // Scheduler for reboot notification and dialog. Unowned.
  raw_ptr<RebootNotificationsScheduler> notifications_scheduler_;

  // Indicating if the reboot should be skipped.
  bool skip_reboot_ = false;

  // Returns device's boot timestamp. The functor is used because the boot time
  // is not constant and can change at runtime, e.g. because of the time
  // sync.
  GetBootTimeCallback get_boot_time_callback_;

  // Observation of chromeos::PowerManagerClient.
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      observation_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_DEVICE_SCHEDULED_REBOOT_HANDLER_H_
