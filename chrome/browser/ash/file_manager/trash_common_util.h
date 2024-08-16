// Copyright 2022 The Chromium Authors
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

// Constant representing the extended attribute "user.TrackedDirectoryName" used
// to track the files and info directories for deletion by cryptohome.
extern const char kTrackedDirectoryName[];

// The histogram used to record the success or failure of the lazy creation of
// the Trash directory and its children.
extern const char kDirectorySetupHistogramName[];

// The histogram used to record the various types of failures that might occur
// when trying to trash an item.
extern const char kFailedTrashingHistogramName[];

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
  // user's local directory and in Downloads to prefix the restoration path
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

// Verify whether trash is enabled by feature flag and whether the enterprise
// policies are not disabling the feature. The policies have dynamic refresh, so
// so this should be checked every time the operation this is guarding is
// executed.
bool IsTrashEnabledForProfile(Profile* profile);

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

// Enum of possible UMA values for histogram FileBrowser.Trash.DirectorySetup.
// Keep the order of this in sync with FileManagerTrashDirectorySetupStep in
// tools/metrics/histograms/enums.xml.
enum class DirectorySetupUmaType {
  FAILED_INFO_FOLDER = 0,
  FAILED_FILES_FOLDER = 1,
  FAILED_XATTR = 2,
  FAILED_PARENT_FOLDER_PERMISSIONS = 3,
  kMaxValue = FAILED_PARENT_FOLDER_PERMISSIONS,
};

// Enum of possible UMA values for histogram FileBrowser.Trash.FailedTrashing.
// Keep the order of this in sync with FailedTrashingType in
// tools/metrics/histograms/enums.xml.
enum class FailedTrashingUmaType {
  FAILED_WRITING_METADATA = 0,
  FAILED_MOVING_FILE = 1,
  kMaxValue = FAILED_MOVING_FILE,
};

}  // namespace file_manager::trash

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_COMMON_UTIL_H_
