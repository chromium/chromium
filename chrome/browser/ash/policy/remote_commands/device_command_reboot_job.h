// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REBOOT_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REBOOT_JOB_H_

#include "base/macros.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace chromeos {

class PowerManagerClient;

}  // namespace chromeos

namespace policy {

class DeviceCommandRebootJob : public RemoteCommandJob {
 public:
  explicit DeviceCommandRebootJob(
      chromeos::PowerManagerClient* power_manager_client);
  ~DeviceCommandRebootJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 private:
  // RemoteCommandJob:
  void RunImpl(CallbackWithResult succeeded_callback,
               CallbackWithResult failed_callback) override;

  chromeos::PowerManagerClient* power_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCommandRebootJob);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REBOOT_JOB_H_
