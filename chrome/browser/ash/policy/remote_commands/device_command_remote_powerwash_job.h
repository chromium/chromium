// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REMOTE_POWERWASH_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REMOTE_POWERWASH_JOB_H_

#include "base/memory/raw_ptr.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

class RemoteCommandsService;

class DeviceCommandRemotePowerwashJob : public RemoteCommandJob {
 public:
  explicit DeviceCommandRemotePowerwashJob(RemoteCommandsService* service);

  DeviceCommandRemotePowerwashJob(const DeviceCommandRemotePowerwashJob&) =
      delete;
  DeviceCommandRemotePowerwashJob& operator=(
      const DeviceCommandRemotePowerwashJob&) = delete;

  ~DeviceCommandRemotePowerwashJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 protected:
  // RemoteCommandJob:
  bool IsExpired(base::TimeTicks now) override;
  void RunImpl(CallbackWithResult result_callback) override;

 private:
  const raw_ptr<RemoteCommandsService> service_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REMOTE_POWERWASH_JOB_H_
