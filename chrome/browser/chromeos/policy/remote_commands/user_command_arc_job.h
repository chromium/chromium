// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_USER_COMMAND_ARC_JOB_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_USER_COMMAND_ARC_JOB_H_

#include <string>

#include "base/macros.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

class Profile;

namespace policy {

class UserCommandArcJob : public RemoteCommandJob {
 public:
  explicit UserCommandArcJob(Profile* profile);
  ~UserCommandArcJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;
  base::TimeDelta GetCommandTimeout() const override;

 protected:
  // RemoteCommandJob:
  bool ParseCommandPayload(const std::string& command_payload) override;
  void RunImpl(CallbackWithResult succeeded_callback,
               CallbackWithResult failed_callback) override;

 private:
  Profile* const profile_;
  std::string command_payload_;

  DISALLOW_COPY_AND_ASSIGN(UserCommandArcJob);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_USER_COMMAND_ARC_JOB_H_
