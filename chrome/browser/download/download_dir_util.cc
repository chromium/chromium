// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_dir_util.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/files/file_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/common/chrome_paths_lacros.h"
#include "components/drive/file_system_core_util.h"
#endif

namespace download_dir_util {

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kLocationGoogleDrive[] = "google_drive";
const char kLocationOneDrive[] = "microsoft_onedrive";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
const char kDriveNamePolicyVariableName[] = "${google_drive}";
const char kOneDriveNamePolicyVariableName[] = "${microsoft_onedrive}";

bool DownloadToDrive(const base::FilePath::StringType& string_value,
                     const policy::PolicyHandlerParameters& parameters) {
  const size_t position = string_value.find(kDriveNamePolicyVariableName);
  return (position != base::FilePath::StringType::npos &&
          !parameters.user_id_hash.empty());
}

bool DownloadToOneDrive(const base::FilePath::StringType& string_value,
                        const policy::PolicyHandlerParameters& parameters) {
  const size_t position = string_value.find(kOneDriveNamePolicyVariableName);
  return (position != base::FilePath::StringType::npos &&
          !parameters.user_id_hash.empty());
}

bool ExpandDrivePolicyVariable(Profile* profile,
                               const base::FilePath& old_path,
                               base::FilePath* new_path) {
  size_t position = old_path.value().find(kDriveNamePolicyVariableName);
  if (position == base::FilePath::StringType::npos)
    return false;

  base::FilePath google_drive;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!integration_service || !integration_service->is_enabled())
    return false;
  google_drive = integration_service->GetMountPointPath();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  bool drivefs_mounted = chrome::GetDriveFsMountPointPath(&google_drive);
  if (!drivefs_mounted)
    return false;
#endif

  base::FilePath::StringType google_drive_root =
      google_drive.Append(drive::util::kDriveMyDriveRootDirName).value();
  std::string expanded_value = old_path.value();
  *new_path = base::FilePath(expanded_value.replace(
      position, strlen(kDriveNamePolicyVariableName), google_drive_root));
  return true;
}

bool ExpandOneDrivePolicyVariable(Profile* profile,
                                  const base::FilePath& old_path,
                                  base::FilePath* new_path) {
  if (!base::FeatureList::IsEnabled(features::kSkyVault)) {
    return false;
  }

  size_t position = old_path.value().find(kOneDriveNamePolicyVariableName);
  if (position == base::FilePath::StringType::npos) {
    return false;
  }

  base::FilePath onedrive_path;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::GetTempDir(&onedrive_path)) {
    return false;
  }

#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  bool onedrive_mounted = chrome::GetOneDriveMountPointPath(&onedrive_path);
  if (!onedrive_mounted) {
    return false;
  }
#endif

  std::string expanded_value = old_path.value();
  *new_path =
      base::FilePath(expanded_value.replace(
                         position, strlen(kOneDriveNamePolicyVariableName),
                         onedrive_path.value()))
          .StripTrailingSeparators();
  return true;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

base::FilePath::StringType ExpandDownloadDirectoryPath(
    const base::FilePath::StringType& string_value,
    const policy::PolicyHandlerParameters& parameters) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return string_value;
#else
  return policy::path_parser::ExpandPathVariables(string_value);
#endif
}

}  // namespace download_dir_util
