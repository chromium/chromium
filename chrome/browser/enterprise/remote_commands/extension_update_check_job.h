// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_EXTENSION_UPDATE_CHECK_JOB_H_
#define CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_EXTENSION_UPDATE_CHECK_JOB_H_

#include <string>

#include "chrome/browser/enterprise/remote_commands/job_profile_picker.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "extensions/common/extension_id.h"

class ProfileManager;
class Profile;

namespace enterprise_commands {

// A remote command for an extension update check in a specific profile.
class ExtensionUpdateCheckJob : public policy::RemoteCommandJob {
 public:
  explicit ExtensionUpdateCheckJob(ProfileManager* profile_manager);
  explicit ExtensionUpdateCheckJob(Profile* profile);
  ~ExtensionUpdateCheckJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 protected:
  // RemoteCommandJob:
  bool ParseCommandPayload(const std::string& command_payload) override;
  void RunImpl(CallbackWithResult result_callback) override;

 private:
  JobProfilePicker job_profile_picker_;
};

}  // namespace enterprise_commands

#endif  // CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_EXTENSION_UPDATE_CHECK_JOB_H_
