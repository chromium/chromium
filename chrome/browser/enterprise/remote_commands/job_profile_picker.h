// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_JOB_PROFILE_PICKER_H_
#define CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_JOB_PROFILE_PICKER_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class ProfileManager;
class Profile;

namespace enterprise_commands {
// A helper class that allow remote command job select the correct profile to
// execute the command, regardless the command come with CBCM or Profile
// management.
class JobProfilePicker {
 public:
  explicit JobProfilePicker(Profile* profile);
  explicit JobProfilePicker(ProfileManager* profile);
  JobProfilePicker(const JobProfilePicker&) = delete;
  JobProfilePicker& operator=(const JobProfilePicker&) = delete;
  ~JobProfilePicker();

  bool ParseCommandPayload(const base::Value::Dict& command_payload);
  Profile* GetProfile();

 private:
  absl::variant<raw_ptr<Profile>, raw_ptr<ProfileManager>>
      profile_or_profile_manager_;

  base::FilePath profile_path_;
};

}  // namespace enterprise_commands

#endif  // CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_JOB_PROFILE_PICKER_H_
