// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/file_location_utils.h"

#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/handlers/screen_capture_location_policy_handler.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace policy::local_user_files {

namespace {

constexpr char kOdfsVirtualPath[] = "/odfs-virtual-path";

base::FilePath GetDriveFsMountPointPath() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile) {
    return base::FilePath();
  }
  auto* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!integration_service || !integration_service->is_enabled()) {
    return base::FilePath();
  }
  return integration_service->GetMountPointPath();
}

base::FilePath GetUserDefaultDownloadsFolder() {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  return profile ? file_manager::util::GetDownloadsFolderForProfile(profile)
                 : base::FilePath();
}

}  // namespace

base::FilePath GetODFSVirtualPath() {
  return base::FilePath(kOdfsVirtualPath);
}

// The location string may have Google Drive or Microsoft Drive placeholders,
// but only in the beginning, so checking that if found - at start.
bool IsValidLocationString(const std::string& str) {
  const size_t google_drive_position = str.find(kGoogleDrivePolicyVariableName);
  if (google_drive_position != std::string::npos &&
      google_drive_position != 0) {
    return false;
  }
  const size_t onedrive_position = str.find(kOneDrivePolicyVariableName);
  if (onedrive_position != std::string::npos && onedrive_position != 0) {
    return false;
  }
  return true;
}

base::FilePath ResolvePath(const std::string& path_str) {
  if (!IsValidLocationString(path_str)) {
    return base::FilePath();
  }

  // Empty path in the policy means default downloads directory.
  if (path_str.empty()) {
    return GetUserDefaultDownloadsFolder();
  }

  const size_t google_drive_position =
      path_str.find(kGoogleDrivePolicyVariableName);
  if (google_drive_position != std::string::npos) {
    base::FilePath drive_path = GetDriveFsMountPointPath();
    if (drive_path.empty()) {
      // Returning default if Google Drive can't be found.
      return base::FilePath();
    }
    std::string result_str = path_str;
    const base::FilePath resolved =
        base::FilePath(
            result_str.replace(google_drive_position,
                               strlen(kGoogleDrivePolicyVariableName),
                               drive_path.Append("root").AsUTF8Unsafe()))
            .StripTrailingSeparators();
    return resolved;
  }

  const size_t one_drive_position = path_str.find(kOneDrivePolicyVariableName);
  if (one_drive_position != std::string::npos) {
    // Never empty.
    const base::FilePath one_drive_path = GetODFSVirtualPath();
    std::string result_str = path_str;
    const base::FilePath resolved =
        base::FilePath(result_str.replace(one_drive_position,
                                          strlen(kOneDrivePolicyVariableName),
                                          one_drive_path.AsUTF8Unsafe()))
            .StripTrailingSeparators();
    return resolved;
  }

  return base::FilePath(path_str);
}

}  // namespace policy::local_user_files
