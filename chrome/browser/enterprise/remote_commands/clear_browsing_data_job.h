// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_CLEAR_BROWSING_DATA_JOB_H_
#define CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_CLEAR_BROWSING_DATA_JOB_H_

#include "base/files/file_path.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "content/public/browser/browsing_data_remover.h"

class ProfileManager;

namespace enterprise_commands {

// A remote command for clearing browsing data associated with a specific
// profile.
class ClearBrowsingDataJob : public policy::RemoteCommandJob,
                             public content::BrowsingDataRemover::Observer {
 public:
  explicit ClearBrowsingDataJob(ProfileManager* profile_manager);
  ~ClearBrowsingDataJob() override;

 private:
  class ResultPayload : public RemoteCommandJob::ResultPayload {
   public:
    explicit ResultPayload(uint64_t failed_data_types);
    ~ResultPayload() override;

   private:
    // Define the possibly failed data types here for 2 reasons:
    //
    // 1. This will be easier to keep in sync with the server, as the latter
    // doesn't care about *all* the types in BrowsingDataRemover.
    //
    // 2. Centralize handling the underlying type of the values here.
    // BrowsingDataRemover represents failed types as uint64_t, which isn't
    // natively supported by base::Value, so this class needs to convert to a
    // type that's supported. This will also allow us to use a list instead of a
    // bit mask, which will be easier to parse gracefully on the server in case
    // more types are added.
    enum DataTypes {
      CACHE = 0,
      COOKIES = 1,
    };

    std::unique_ptr<std::string> Serialize() override;

    uint64_t failed_data_types_;
  };

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;
  bool ParseCommandPayload(const std::string& command_payload) override;
  void RunImpl(CallbackWithResult succeeded_callback,
               CallbackWithResult failed_callback) override;

  // content::BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override;

  base::FilePath profile_path_;
  bool clear_cache_;
  bool clear_cookies_;

  // RunImpl callbacks which will be invoked by OnBrowsingDataRemoverDone.
  CallbackWithResult succeeded_callback_;
  CallbackWithResult failed_callback_;

  // Non-owned pointer to the ProfileManager of the current browser process.
  ProfileManager* profile_manager_;
};

}  // namespace enterprise_commands

#endif  // CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_CLEAR_BROWSING_DATA_JOB_H_
