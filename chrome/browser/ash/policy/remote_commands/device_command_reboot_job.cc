// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_reboot_job.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/cros_system_api/dbus/power_manager/dbus-constants.h"

namespace policy {

namespace {

const char kKioskRebootDescription[] = "Reboot remote command (kiosk)";
const char kLoginScreenRebootDescription[] =
    "Reboot remote command (login screen)";

base::TimeTicks GetBootTime() {
  return base::TimeTicks::Now() - base::SysInfo::Uptime();
}

}  // namespace

DeviceCommandRebootJob::DeviceCommandRebootJob()
    : DeviceCommandRebootJob(chromeos::PowerManagerClient::Get(),
                             ash::LoginState::Get(),
                             ash::SessionTerminationManager::Get(),
                             base::BindRepeating(GetBootTime)) {}

DeviceCommandRebootJob::DeviceCommandRebootJob(
    chromeos::PowerManagerClient* power_manager_client,
    ash::LoginState* loging_state,
    ash::SessionTerminationManager* session_termination_manager,
    GetBootTimeCallback get_boot_time_callback)
    : power_manager_client_(power_manager_client),
      login_state_(loging_state),
      session_termination_manager_(session_termination_manager),
      get_boot_time_callback_(std::move(get_boot_time_callback)) {
  DCHECK(get_boot_time_callback_);
}

DeviceCommandRebootJob::~DeviceCommandRebootJob() = default;

enterprise_management::RemoteCommand_Type DeviceCommandRebootJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_DEVICE_REBOOT;
}

void DeviceCommandRebootJob::RunImpl(CallbackWithResult result_callback) {
  result_callback_ = std::move(result_callback);

  // Determines the time delta between the command having been issued and the
  // boot time of the system.
  const base::TimeDelta delta = get_boot_time_callback_.Run() - issued_time();
  // If the reboot command was issued before the system booted, we inform the
  // server that the reboot succeeded. Otherwise, the reboot must still be
  // performed and we invoke it.
  if (delta.is_positive()) {
    LOG(WARNING) << "Ignoring reboot command issued " << delta
                 << " before current boot time";
    RunAsyncCallback(std::move(result_callback_), FROM_HERE);
    return;
  }

  // The device is able to reboot immediately if it has no ongoing user session:
  // if it runs in kiosk mode or is on login screen.
  if (login_state_->IsKioskSession()) {
    DoReboot(kKioskRebootDescription);
    return;
  }

  if (!login_state_->IsUserLoggedIn()) {
    DoReboot(kLoginScreenRebootDescription);
    return;
  }

  RebootUserSession();
}

void DeviceCommandRebootJob::RebootUserSession() {
  // TODO(b/265784089): Make reboot on user logout robust. If the browser
  // crashes, all the reboot information is gone while it should be preserved.

  session_termination_manager_->SetDeviceRebootOnSignoutForRemoteCommand(
      base::BindOnce(&DeviceCommandRebootJob::OnSignout,
                     weak_factory_.GetWeakPtr()));
}

void DeviceCommandRebootJob::OnSignout() {
  // `session_termination_manager_` will initiate the reboot, just report the
  // command finished.
  RunAsyncCallback(std::move(result_callback_), FROM_HERE);
}

void DeviceCommandRebootJob::DoReboot(const std::string& reason) {
  DCHECK(result_callback_);

  // Posting the task with a callback just before reboot request does not
  // guarantee the callback reaching `RemoteCommandsService` and is very
  // unlikely to be reported to DMServer. So the callback is mostly used for
  // testing purposes.
  // TODO(b/252980103): Come up with a mechanism to deliver the execution result
  // to DMServer.
  RunAsyncCallback(std::move(result_callback_), FROM_HERE);
  power_manager_client_->RequestRestart(
      power_manager::REQUEST_RESTART_REMOTE_ACTION_REBOOT, reason);
}

// static
void DeviceCommandRebootJob::RunAsyncCallback(CallbackWithResult callback,
                                              base::Location from_where) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      from_where,
      base::BindOnce(std::move(callback), ResultType::kSuccess, absl::nullopt));
}

}  // namespace policy
