// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REBOOT_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REBOOT_JOB_H_

#include <string>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace ash {
class LoginState;
class SessionTerminationManager;
}  // namespace ash

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace policy {

class RebootNotificationsScheduler;

// Reboots a device with regards to its current mode. See
// go/cros-reboot-command-dd for detailed design. Handles the following cases:
// * If the device was booted after the command was issued: does not reboot and
//   reports success.
// * If the power manager service is unavailable, reports failure.
// * If the devices runs in a kiosk mode, reports success and reboots
//   immediately.
// * If the device runs in a regular mode:
//   * If there is no logged in user, reports success and reboots immediately.
//   * If a user is logged in, notifies the user, waits for a timeout, reports
//     success and reboots.
//   * If the user signs out during the waiting period, reports success and
//     reboots.
class DeviceCommandRebootJob : public RemoteCommandJob,
                               public chromeos::PowerManagerClient::Observer {
 public:
  DeviceCommandRebootJob();

  DeviceCommandRebootJob(const DeviceCommandRebootJob&) = delete;
  DeviceCommandRebootJob& operator=(const DeviceCommandRebootJob&) = delete;

  ~DeviceCommandRebootJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

  // chromeos::PowerManagerClient::Observer:
  void PowerManagerBecameAvailable(bool available) override;

 protected:
  using GetBootTimeCallback = base::RepeatingCallback<base::TimeTicks()>;

  // Extended constructor for testing puproses.
  // Extended constructor for testing puproses. `power_manager_client`,
  // `loging_state`, `session_termination_manager`,
  // `in_session_notifications_scheduler`, `clock` and `tick_clock` must
  // outlive the job.
  DeviceCommandRebootJob(
      chromeos::PowerManagerClient* power_manager_client,
      ash::LoginState* loging_state,
      ash::SessionTerminationManager* session_termination_manager,
      RebootNotificationsScheduler* in_session_notifications_scheduler,
      const base::Clock* clock,
      const base::TickClock* tick_clock,
      GetBootTimeCallback get_boot_time_callback);

  bool ParseCommandPayload(const std::string& command_payload) override;

 private:
  // Posts a task with a callback. Command's callbacks cannot be run
  // synchronously from `RunImpl`.
  static void RunAsyncCallback(CallbackWithResult callback,
                               ResultType result,
                               base::Location from_where);

  // RemoteCommandJob:
  void RunImpl(CallbackWithResult result_callback) override;

  // Handles reboot with an active user. Shows a reboot notification, waits for
  // the timeout or sign out, and reboots.
  void RebootUserSession();

  // Called when `session_termination_manager_` is about to reboot on signout.
  void OnSignout();

  // Called when a user clicks reboot button on the reboot dialog.
  void OnRebootButtonClicked();

  void OnRebootTimeoutExpired();

  // Reports success and initiates a reboot request with given `reason`.
  // Shall be called once.
  void DoReboot(const std::string& reason);

  // Unsubscribes from events that trigger reboot, e.g. in-session timer.
  void ResetTriggeringEvents();

  // Sends the reboot request to power manager service. Unowned.
  const raw_ptr<chromeos::PowerManagerClient> power_manager_client_;
  // Checks the availability of `power_manager_client_`.
  base::ScopedObservation<chromeos::PowerManagerClient, DeviceCommandRebootJob>
      power_manager_availability_observation_{this};

  // Provides information about current logins status and device mode to
  // determine how to proceed with the reboot.
  const raw_ptr<ash::LoginState> login_state_;

  // Handles reboot on signout.
  raw_ptr<ash::SessionTerminationManager> session_termination_manager_;

  // Scheduler for reboot notification and dialog. Unowned.
  raw_ptr<RebootNotificationsScheduler> in_session_notifications_scheduler_;
  // Timer tracking the delayed reboot event.
  base::WallClockTimer in_session_reboot_timer_;

  // Clock to schedule in-user-session reboot delay. Can be mocked for testing.
  // Unowned.
  raw_ptr<const base::Clock> clock_;

  // Returns device's boot timestamp. The boot time is not constant and may
  // change at runtime, e.g. because of time sync.
  const GetBootTimeCallback get_boot_time_callback_;

  CallbackWithResult result_callback_;

  // Delay between execution start in user session and the reboot.
  base::TimeDelta user_session_delay_;

  base::WeakPtrFactory<DeviceCommandRebootJob> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REBOOT_JOB_H_
