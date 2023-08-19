// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_CLEAR_BROWSING_DATA_JOB_H_
#define CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_CLEAR_BROWSING_DATA_JOB_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/remote_commands/job_profile_picker.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "content/public/browser/browsing_data_remover.h"

class ProfileManager;
class Profile;

namespace enterprise_commands {

// A remote command for clearing browsing data associated with a specific
// profile.
class ClearBrowsingDataJob : public policy::RemoteCommandJob,
                             public content::BrowsingDataRemover::Observer {
 public:
  explicit ClearBrowsingDataJob(ProfileManager* profile_manager);
  explicit ClearBrowsingDataJob(Profile* profile);
  ~ClearBrowsingDataJob() override;

 private:
  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;
  bool ParseCommandPayload(const std::string& command_payload) override;
  void RunImpl(CallbackWithResult result_callback) override;

  // content::BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override;

  bool clear_cache_;
  bool clear_cookies_;

  // RunImpl callback which will be invoked by OnBrowsingDataRemoverDone.
  CallbackWithResult result_callback_;
  JobProfilePicker job_profile_picker_;
};

}  // namespace enterprise_commands

#endif  // CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_CLEAR_BROWSING_DATA_JOB_H_
