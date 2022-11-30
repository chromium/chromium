// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_USER_COMMAND_ARC_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_USER_COMMAND_ARC_JOB_H_

#include <string>

#include "components/policy/core/common/remote_commands/remote_command_job.h"

class Profile;

namespace policy {

class UserCommandArcJob : public RemoteCommandJob {
 public:
  explicit UserCommandArcJob(Profile* profile);

  UserCommandArcJob(const UserCommandArcJob&) = delete;
  UserCommandArcJob& operator=(const UserCommandArcJob&) = delete;

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
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_USER_COMMAND_ARC_JOB_H_
