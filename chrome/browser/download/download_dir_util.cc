// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_dir_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif  // defined(OS_CHROMEOS)

namespace {
#if defined(OS_CHROMEOS)
// Drive root folder relative to its mount point.
const base::FilePath::CharType* kRootRelativeToDriveMount =
    FILE_PATH_LITERAL("root");
#endif  // defined(OS_CHROMEOS)
}  // namespace

namespace download_dir_util {

#if defined(OS_CHROMEOS)
const char kDriveNamePolicyVariableName[] = "${google_drive}";

bool DownloadToDrive(const base::FilePath::StringType& string_value,
                     const policy::PolicyHandlerParameters& parameters) {
  const size_t position = string_value.find(kDriveNamePolicyVariableName);
  return (position != base::FilePath::StringType::npos &&
          !parameters.user_id_hash.empty());
}

bool ExpandDrivePolicyVariable(Profile* profile,
                               const base::FilePath& old_path,
                               base::FilePath* new_path) {
  auto* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!integration_service || !integration_service->is_enabled())
    return false;

  size_t position = old_path.value().find(kDriveNamePolicyVariableName);
  if (position == base::FilePath::StringType::npos)
    return false;

  base::FilePath::StringType google_drive_root =
      integration_service->GetMountPointPath()
          .Append(kRootRelativeToDriveMount)
          .value();
  std::string expanded_value = old_path.value();
  *new_path = base::FilePath(expanded_value.replace(
      position, strlen(kDriveNamePolicyVariableName), google_drive_root));
  return true;
}
#endif  // defined(OS_CHROMEOS)

base::FilePath::StringType ExpandDownloadDirectoryPath(
    const base::FilePath::StringType& string_value,
    const policy::PolicyHandlerParameters& parameters) {
#if defined(OS_CHROMEOS)
  return string_value;
#else
  return policy::path_parser::ExpandPathVariables(string_value);
#endif
}

}  // namespace download_dir_util
