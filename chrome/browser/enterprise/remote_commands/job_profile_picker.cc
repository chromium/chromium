// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/job_profile_picker.h"

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace enterprise_commands {
namespace {

const char kProfilePathField[] = "profile_path";

}  // namespace

JobProfilePicker::JobProfilePicker(Profile* profile)
    : profile_or_profile_manager_(profile) {}
JobProfilePicker::JobProfilePicker(ProfileManager* profile_manager)
    : profile_or_profile_manager_(profile_manager) {}

JobProfilePicker::~JobProfilePicker() = default;

bool JobProfilePicker::ParseCommandPayload(
    const base::Value::Dict& command_payload) {
  if (absl::holds_alternative<raw_ptr<Profile>>(profile_or_profile_manager_)) {
    return true;
  }

  const std::string* path = command_payload.FindString(kProfilePathField);
  if (!path) {
    return false;
  }

  // On Windows, file paths are wstring as opposed to string on other platforms.
  // On POSIX platforms other than MacOS and ChromeOS, the encoding is unknown.
  //
  // This path is sent from the server, which obtained it from Chrome in a
  // previous report, and Chrome casts the path as UTF8 using UTF8Unsafe before
  // sending it (see BrowserReportGeneratorDesktop::GenerateProfileInfo).
  // Because of that, the best thing we can do everywhere is try to get the
  // path from UTF8, and ending up with an invalid path will fail later in
  // RunImpl when we attempt to get the profile from the path.
  profile_path_ = base::FilePath::FromUTF8Unsafe(*path);
#if BUILDFLAG(IS_WIN)
  // For Windows machines, the path that Chrome reports for the profile is
  // "Normalized" to all lower-case on the reporting server. This means that
  // when the server sends the command, the path will be all lower case and
  // the profile manager won't be able to use it as a key. To avoid this issue,
  // This code will iterate over all profile paths and find the one that matches
  // in a case-insensitive comparison. If this doesn't find one, RunImpl will
  // fail in the same manner as if the profile didn't exist, which is the
  // expected behavior.
  ProfileAttributesStorage& storage =
      absl::get<raw_ptr<ProfileManager>>(profile_or_profile_manager_)
          ->GetProfileAttributesStorage();
  for (ProfileAttributesEntry* entry : storage.GetAllProfilesAttributes()) {
    base::FilePath entry_path = entry->GetPath();

    if (base::FilePath::CompareEqualIgnoreCase(profile_path_.value(),
                                               entry_path.value())) {
      profile_path_ = entry_path;
      break;
    }
  }
#endif
  return true;
}

Profile* JobProfilePicker::GetProfile() {
  if (absl::holds_alternative<raw_ptr<Profile>>(profile_or_profile_manager_)) {
    return absl::get<raw_ptr<Profile>>(profile_or_profile_manager_);
  }
  return absl::get<raw_ptr<ProfileManager>>(profile_or_profile_manager_)
      ->GetProfileByPath(profile_path_);
}

}  // namespace enterprise_commands
