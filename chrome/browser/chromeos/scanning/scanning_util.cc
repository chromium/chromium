// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {
namespace scanning {

namespace {

// Path to the active user's "My files" folder.
constexpr char kActiveUserMyFilesPath[] = "/home/chronos/user/MyFiles";

// Determines if |path_to_file| is a supported file path for the Files app. Only
// files under the |drive_path| and |my_files_path| paths are allowed to be
// opened to from the Scan app.
bool FilePathSupported(const base::FilePath& drive_path,
                       const base::FilePath& my_files_path,
                       const base::FilePath& path_to_file) {
  // Don't allow file paths with "../path".
  if (path_to_file.ReferencesParent())
    return false;

  if (drive_path.IsParent(path_to_file))
    return true;

  if (path_to_file.DirName() == base::FilePath(kActiveUserMyFilesPath) ||
      my_files_path.IsParent(path_to_file))
    return true;

  return false;
}

}  // namespace

bool ShowFileInFilesApp(const base::FilePath& drive_path,
                        const base::FilePath& my_files_path,
                        content::WebUI* web_ui,
                        const base::FilePath& path_to_file) {
  if (!FilePathSupported(drive_path, my_files_path, path_to_file))
    return false;

  if (!base::PathExists(path_to_file))
    return false;

  // Replace the MyFiles alias with the full MyFiles path.
  base::FilePath path_to_use(path_to_file);
  if (path_to_use.DirName().value() == kActiveUserMyFilesPath) {
    path_to_use = my_files_path.Append(path_to_file.BaseName());
  }

  platform_util::ShowItemInFolder(Profile::FromWebUI(web_ui), path_to_use);
  return true;
}

base::FilePath GetDrivePath(Profile* profile) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile);
  bool drive_available = integration_service &&
                         integration_service->is_enabled() &&
                         integration_service->IsMounted();
  return drive_available ? integration_service->GetMountPointPath()
                         : base::FilePath();
}

}  // namespace scanning
}  // namespace chromeos
