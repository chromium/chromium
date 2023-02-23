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
#include "base/time/time.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace ash {
class LoginState;
class SessionTerminationManager;
}  // namespace ash

namespace chromeos {
class PowerManagerClient;
}  // namespace chromeos

namespace policy {

// Reboots a device with regards to its current mode. See
// go/cros-reboot-command-dd for detailed design. Handles the following cases:
// * If the device was booted after the command was issued: does not reboot and
//   reports success.
// * If the devices runs in a kiosk mode, reports success and reboots
//   immediately.
// * If the device runs in a regular mode:
//   * If there is no logged in user, reports success and reboots immediately.
//   * If the user signs out, reports success and reboots.
class DeviceCommandRebootJob : public RemoteCommandJob {
 public:
  DeviceCommandRebootJob();

  DeviceCommandRebootJob(const DeviceCommandRebootJob&) = delete;
  DeviceCommandRebootJob& operator=(const DeviceCommandRebootJob&) = delete;

  ~DeviceCommandRebootJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 protected:
  using GetBootTimeCallback = base::RepeatingCallback<base::TimeTicks()>;

  // Extended constructor for testing puproses.
  DeviceCommandRebootJob(
      chromeos::PowerManagerClient* power_manager_client,
      ash::LoginState* loging_state,
      ash::SessionTerminationManager* session_termination_manager,
      GetBootTimeCallback get_boot_time_callback);

 private:
  // Posts a task with a callback. Command's callbacks cannot be run
  // synchronously from `RunImpl`.
  static void RunAsyncCallback(CallbackWithResult callback,
                               base::Location from_where);

  // RemoteCommandJob:
  void RunImpl(CallbackWithResult result_callback) override;

  // Reboots the device on user logout.
  void RebootUserSession();

  // Called when `session_termination_manager_` is about to reboot on signout.
  void OnSignout();

  // Reports success and initiates a reboot request with given `reason`.
  // Shall be called once.
  void DoReboot(const std::string& reason);

  // TODO(b/265784089): `DeviceCommandRebootJob` should track the availability
  // status. The client might not be available at the time the command is
  // executed. The issue is that the client reports available status when
  // requested and not available status only when it is first requested. This
  // may lead to the command waiting for the status forever.
  const base::raw_ptr<chromeos::PowerManagerClient> power_manager_client_;

  // Provides information about current logins status and device mode to
  // determine how to proceed with the reboot.
  const base::raw_ptr<ash::LoginState> login_state_;

  // Handles reboot on signout.
  base::raw_ptr<ash::SessionTerminationManager> session_termination_manager_;

  // Returns device's boot timestamp. The boot time is not constant and may
  // change at runtime, e.g. because of time sync.
  const GetBootTimeCallback get_boot_time_callback_;

  CallbackWithResult result_callback_;

  base::WeakPtrFactory<DeviceCommandRebootJob> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REBOOT_JOB_H_
