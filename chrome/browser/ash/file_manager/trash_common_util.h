// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_COMMON_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_COMMON_UTIL_H_

#include <map>

#include "base/files/file_path.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace file_manager::trash {

// Constant representing the Trash folder name.
extern const char kTrashFolderName[];

// Constant representing the "info" folder name inside .Trash.
extern const char kInfoFolderName[];

// Constant representing the "files" folder name inside .Trash.
extern const char kFilesFolderName[];

// Constant representing the ".trashinfo" extension for metadata files.
extern const char kTrashInfoExtension[];

struct TrashLocation {
  TrashLocation(const base::FilePath supplied_relative_folder_path,
                const base::FilePath supplied_mount_point_path,
                const base::FilePath prefix_path);
  // Constructor used when no prefix path is required.
  TrashLocation(const base::FilePath supplied_relative_folder_path,
                const base::FilePath supplied_mount_point_path);
  ~TrashLocation();

  TrashLocation(TrashLocation&& other);
  TrashLocation& operator=(TrashLocation&& other);

  // The location of the .Trash/files folder.
  storage::FileSystemURL trash_files;

  // The location of the .Trash/info folder.
  storage::FileSystemURL trash_info;

  // The folder path for the Trash folder. This is parented by
  // `mount_point_path` and typically represents the .Trash folder. However, in
  // some cases this can represent a path instead. This path must be relative
  // from the `mount_point_path`, i.e. not an absolute path.
  base::FilePath relative_folder_path;

  // The volume mount point for the trash folder. For example the Downloads and
  // MyFiles entries have the same mount point path (~/MyFiles).
  base::FilePath mount_point_path;

  // For some trash directories, the restore path requires a prefix to ensure
  // restoration is done correctly. This is used in Crostini to denote the
  // user's local directory and in Downloads to prefix the reestoration path
  // with /Downloads as MyFiles and Downloads share the same mount point. This
  // prefix is prepended to the restore path when creating out the .trashinfo
  // file.
  base::FilePath prefix_restore_path;

  // The free space on the underlying filesystem that .Trash is located on.
  int64_t free_space;

  // Whether this directory require setting up. This is enabled once free space
  // has been retrieved for the underlying file system. If false directory setup
  // is skipped.
  bool require_setup = false;
};

// Helper to create a destination path for a file in one of the .Trash folders.
const base::FilePath GenerateTrashPath(const base::FilePath& trash_path,
                                       const std::string& subdir,
                                       const std::string& file_name);

// Generate the list of currently enabled paths for trashing. It includes
// DriveFS and Crostini paths when they're enabled. The key is the parent path
// where the `relative_folder_path` is located, this is used to match the trash
// location to trashed files. The entries can contain nested folders (e.g.
// ~/MyFiles and ~/MyFiles/Downloads) so it is important to order them with
// parents folders preceding children. The `mount_point_path` is used to
// identify locations that share the same volume.
using TrashPathsMap = std::map<const base::FilePath, TrashLocation>;
TrashPathsMap GenerateEnabledTrashLocationsForProfile(
    Profile* profile,
    const base::FilePath& base_path);

}  // namespace file_manager::trash

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_COMMON_UTIL_H_
