// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_BROWSER_RESTART_JOB_H_
#define CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_BROWSER_RESTART_JOB_H_

#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace enterprise_commands {

// A remote command for restarting the browser.
class BrowserRestartJob : public policy::RemoteCommandJob {
 public:
  BrowserRestartJob();
  ~BrowserRestartJob() override;

 private:
  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;
  bool ParseCommandPayload(const std::string& command_payload) override;
  void RunImpl(CallbackWithResult result_callback) override;
};

}  // namespace enterprise_commands

#endif  // CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_BROWSER_RESTART_JOB_H_
