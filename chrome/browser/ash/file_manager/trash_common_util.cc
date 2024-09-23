// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_common_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "components/prefs/pref_service.h"

namespace file_manager::trash {

constexpr char kTrashFolderName[] = ".Trash";
constexpr char kInfoFolderName[] = "info";
constexpr char kFilesFolderName[] = "files";
constexpr char kTrashInfoExtension[] = ".trashinfo";
constexpr char kTrackedDirectoryName[] = "user.TrackedDirectoryName";
constexpr char kDirectorySetupHistogramName[] =
    "FileBrowser.Trash.DirectorySetupFailed";
constexpr char kFailedTrashingHistogramName[] =
    "FileBrowser.Trash.FailedTrashing";

TrashLocation::TrashLocation(const base::FilePath supplied_relative_folder_path,
                             const base::FilePath supplied_mount_point_path,
                             const base::FilePath prefix_path)
    : relative_folder_path(supplied_relative_folder_path),
      mount_point_path(supplied_mount_point_path),
      prefix_restore_path(prefix_path) {}

TrashLocation::TrashLocation(const base::FilePath supplied_relative_folder_path,
                             const base::FilePath supplied_mount_point_path)
    : relative_folder_path(supplied_relative_folder_path),
      mount_point_path(supplied_mount_point_path) {}
TrashLocation::~TrashLocation() = default;

TrashLocation::TrashLocation(TrashLocation&& other) = default;
TrashLocation& TrashLocation::operator=(TrashLocation&& other) = default;

bool IsTrashEnabledForProfile(Profile* profile) {
  if (!profile || !profile->GetPrefs()) {
    return false;
  }
  return profile->GetPrefs()->GetBoolean(ash::prefs::kFilesAppTrashEnabled) &&
         policy::local_user_files::LocalUserFilesAllowed();
}

const base::FilePath GenerateTrashPath(const base::FilePath& trash_path,
                                       const std::string& subdir,
                                       const std::string& file_name) {
  base::FilePath path = trash_path.Append(subdir).Append(file_name);
  // The metadata file in .Trash/info always has the .trashinfo extension.
  if (subdir == kInfoFolderName) {
    path = path.AddExtension(kTrashInfoExtension);
  }
  return path;
}

TrashPathsMap GenerateEnabledTrashLocationsForProfile(
    Profile* profile,
    const base::FilePath& base_path) {
  TrashPathsMap enabled_trash_locations;

  enabled_trash_locations.try_emplace(
      util::GetMyFilesFolderForProfile(profile),
      TrashLocation(
          /*supplied_relative_folder_path=*/base::FilePath(kTrashFolderName),
          /*supplied_mount_point_path=*/util::GetMyFilesFolderForProfile(
              profile)));
  enabled_trash_locations.try_emplace(
      util::GetDownloadsFolderForProfile(profile),
      TrashLocation(
          /*supplied_relative_folder_path=*/base::FilePath(kTrashFolderName),
          /*supplied_mount_point_path=*/
          util::GetMyFilesFolderForProfile(profile),
          /*prefix_path=*/
          util::GetDownloadsFolderForProfile(profile).BaseName()));

  if (base::FeatureList::IsEnabled(ash::features::kFilesTrashDrive)) {
    auto* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile);
    if (integration_service) {
      enabled_trash_locations.try_emplace(
          integration_service->GetMountPointPath(),
          TrashLocation(
              /*supplied_relative_folder_path=*/base::FilePath(".Trash-1000"),
              /*supplied_mount_point_path=*/integration_service
                  ->GetMountPointPath()));
    }
  }

  return enabled_trash_locations;
}

}  // namespace file_manager::trash
