// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIR_UTIL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIR_UTIL_H_

#include "base/files/file_path.h"

class Profile;

namespace policy {
struct PolicyHandlerParameters;
}  // namespace policy

namespace download_dir_util {

#if defined(OS_CHROMEOS)
extern const char kDriveNamePolicyVariableName[];

// Returns whether |string_value| points to a directory in Drive or not.
bool DownloadToDrive(const base::FilePath::StringType& string_value,
                     const policy::PolicyHandlerParameters& parameters);

// Expands the google drive policy variable to the drive root path. This cannot
// be done in ExpandDownloadDirectoryPath() as that gets invoked in a policy
// handler, which are run before the profile is registered.
bool ExpandDrivePolicyVariable(Profile* profile,
                               const base::FilePath& old_path,
                               base::FilePath* new_path);
#endif  // defined(OS_CHROMEOS)

// Expands path variables in the download directory path |string_value|.
base::FilePath::StringType ExpandDownloadDirectoryPath(
    const base::FilePath::StringType& string_value,
    const policy::PolicyHandlerParameters& parameters);

}  // namespace download_dir_util

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIR_UTIL_H_
