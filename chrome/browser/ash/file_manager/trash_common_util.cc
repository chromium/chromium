// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_common_util.h"

#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"

namespace file_manager {
namespace io_task {

constexpr char kTrashFolderName[] = ".Trash";
constexpr char kInfoFolderName[] = "info";
constexpr char kFilesFolderName[] = "files";
constexpr char kTrashInfoExtension[] = ".trashinfo";

TrashLocation::TrashLocation(const base::FilePath supplied_relative_folder_path,
                             const base::FilePath parent_path,
                             const base::FilePath prefix_path)
    : relative_folder_path(supplied_relative_folder_path),
      trash_parent_path(parent_path),
      prefix_restore_path(prefix_path) {}

TrashLocation::TrashLocation(const base::FilePath supplied_relative_folder_path,
                             const base::FilePath parent_path)
    : relative_folder_path(supplied_relative_folder_path),
      trash_parent_path(parent_path) {}
TrashLocation::~TrashLocation() = default;

TrashLocation::TrashLocation(TrashLocation&& other) = default;
TrashLocation& TrashLocation::operator=(TrashLocation&& other) = default;

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
      TrashLocation(base::FilePath(kTrashFolderName),
                    util::GetMyFilesFolderForProfile(profile)));
  enabled_trash_locations.try_emplace(
      util::GetDownloadsFolderForProfile(profile),
      TrashLocation(base::FilePath(kTrashFolderName),
                    util::GetDownloadsFolderForProfile(profile),
                    util::GetDownloadsFolderForProfile(profile).BaseName()));

  auto* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (integration_service) {
    enabled_trash_locations.try_emplace(
        integration_service->GetMountPointPath(),
        TrashLocation(base::FilePath(".Trash-1000"),
                      integration_service->GetMountPointPath()));
  }

  // Ensure Crostini is running before adding it as an enabled path.
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(profile);
  if (crostini::CrostiniManager::GetForProfile(profile) &&
      crostini::IsCrostiniRunning(profile) && volume_manager) {
    // A `base_path` is supplied in tests to ensure files are only added to
    // temporary directories. If `base_path` has been supplied, use the mocked
    // volume mount path instead of the real mount path.
    const base::FilePath crostini_mount_point =
        (base_path.empty())
            ? file_manager::util::GetCrostiniMountDirectory(profile)
            : base_path.Append("crostini");
    base::WeakPtr<file_manager::Volume> volume =
        volume_manager->FindVolumeFromPath(crostini_mount_point);
    if (volume) {
      enabled_trash_locations.try_emplace(
          crostini_mount_point,
          TrashLocation(
              base::FilePath(".local").Append("share").Append("Trash"),
              crostini_mount_point, volume->remote_mount_path()));
    }
  }

  return enabled_trash_locations;
}

}  // namespace io_task
}  // namespace file_manager
