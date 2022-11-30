// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_SCANNING_FILE_PATH_HELPER_H_
#define CHROME_BROWSER_ASH_SCANNING_SCANNING_FILE_PATH_HELPER_H_

#include <string>

#include "base/files/file_path.h"

namespace ash {

class ScanningFilePathHelper {
 public:
  ScanningFilePathHelper();
  ScanningFilePathHelper(const base::FilePath& google_drive_path,
                         const base::FilePath& my_files_path);

  ScanningFilePathHelper(ScanningFilePathHelper&& other);
  ScanningFilePathHelper& operator=(ScanningFilePathHelper&& other);

  ~ScanningFilePathHelper();

  // Gets the display name from |path| to show in the Scan To dropdown. Handles
  // the special case of converting the Google Drive root and MyFiles directory
  // to the desired display names "Google Drive" and "My Files" respectively.
  std::string GetBaseNameFromPath(const base::FilePath& path) const;

  // Returns |my_file_path_|.
  base::FilePath GetMyFilesPath() const;

  // Determines if |path_to_file| is a supported file path for the Files app.
  // Only files under the |drive_path_| and |my_files_path_| paths are allowed
  // to be opened to from the Scan app. Paths with references (i.e. "../path")
  // are not supported.
  bool IsFilePathSupported(const base::FilePath& path_to_file) const;

  void SetRemoveableMediaPathForTesting(const base::FilePath& path);

 private:
  // The paths to the user's My files and Google Drive directories.
  base::FilePath google_drive_path_;
  base::FilePath my_files_path_;

  // The root path to the user's connected removable media.
  base::FilePath removable_media_path_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_SCANNING_FILE_PATH_HELPER_H_
