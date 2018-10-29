// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_dir_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/file_system_util.h"
#endif  // defined(OS_CHROMEOS)

namespace {
#if defined(OS_CHROMEOS)
const char kDriveNamePolicyVariableName[] = "${google_drive}";

// Drive root folder relative to its mount point.
const base::FilePath::CharType* kRootRelativeToDriveMount =
    FILE_PATH_LITERAL("root");
#endif  // defined(OS_CHROMEOS)
}  // namespace

namespace download_dir_util {

#if defined(OS_CHROMEOS)
bool DownloadToDrive(const base::FilePath::StringType& string_value,
                     const policy::PolicyHandlerParameters& parameters) {
  const size_t position = string_value.find(kDriveNamePolicyVariableName);
  return (position != base::FilePath::StringType::npos &&
          !parameters.user_id_hash.empty());
}
#endif  // defined(OS_CHROMEOS)

base::FilePath::StringType ExpandDownloadDirectoryPath(
    const base::FilePath::StringType& string_value,
    const policy::PolicyHandlerParameters& parameters) {
#if defined(OS_CHROMEOS)
  // TODO(kaliamoorthi): Clean up policy::path_parser and fold this code
  // into it. http://crbug.com/352627
  size_t position = string_value.find(kDriveNamePolicyVariableName);
  if (position != base::FilePath::StringType::npos) {
    base::FilePath::StringType google_drive_root;
    if (!parameters.user_id_hash.empty()) {
      google_drive_root = drive::util::GetDriveMountPointPathForUserIdHash(
                              parameters.user_id_hash)
                              .Append(kRootRelativeToDriveMount)
                              .value();
    }
    base::FilePath::StringType expanded_value = string_value;  // Mutable copy.
    return expanded_value.replace(
        position,
        base::FilePath::StringType(kDriveNamePolicyVariableName).length(),
        google_drive_root);
  } else {
    return string_value;
  }
#else
  return policy::path_parser::ExpandPathVariables(string_value);
#endif
}

}  // namespace download_dir_util
