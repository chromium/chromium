// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/scanning_file_path_helper.h"

#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// The default root path for connected removable media.
constexpr char kRemovableMediaPath[] = "/media/removable";

// "root" is appended to the user's Google Drive directory to form the
// complete path.
constexpr char kRoot[] = "root";

}  // namespace

ScanningFilePathHelper::ScanningFilePathHelper() = default;
ScanningFilePathHelper::ScanningFilePathHelper(
    const base::FilePath& google_drive_path,
    const base::FilePath& my_files_path)
    : google_drive_path_(google_drive_path),
      my_files_path_(my_files_path),
      removable_media_path_(base::FilePath(kRemovableMediaPath)) {}

ScanningFilePathHelper::ScanningFilePathHelper(ScanningFilePathHelper&& other) =
    default;
ScanningFilePathHelper& ScanningFilePathHelper::operator=(
    ScanningFilePathHelper&& other) = default;

ScanningFilePathHelper::~ScanningFilePathHelper() = default;

std::string ScanningFilePathHelper::GetBaseNameFromPath(
    const base::FilePath& path) const {
  DCHECK(!my_files_path_.empty());

  // Returns string "Google Drive" from path "/media/fuse/drivefs-xxx/root".
  if (!google_drive_path_.empty() && google_drive_path_.Append(kRoot) == path)
    return l10n_util::GetStringUTF8(IDS_SCANNING_APP_MY_DRIVE);

  // Returns string "My Files" from path "/home/chronos/u-xxx/MyFiles".
  if (my_files_path_ == path)
    return l10n_util::GetStringUTF8(IDS_SCANNING_APP_MY_FILES_SELECT_OPTION);

  // Returns base name as is from |path|.
  return path.BaseName().value();
}

base::FilePath ScanningFilePathHelper::GetMyFilesPath() const {
  return my_files_path_;
}

bool ScanningFilePathHelper::IsFilePathSupported(
    const base::FilePath& path_to_file) const {
  DCHECK(!my_files_path_.empty());

  return path_to_file == my_files_path_ ||
         (!path_to_file.ReferencesParent() &&
          ((!google_drive_path_.empty() &&
            google_drive_path_.IsParent(path_to_file)) ||
           my_files_path_.IsParent(path_to_file) ||
           removable_media_path_.IsParent(path_to_file)));
}

void ScanningFilePathHelper::SetRemoveableMediaPathForTesting(
    const base::FilePath& path) {
  removable_media_path_ = path;
}

}  // namespace ash
