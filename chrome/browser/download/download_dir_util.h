// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIR_UTIL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIR_UTIL_H_

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

class Profile;

namespace policy {
struct PolicyHandlerParameters;
}  // namespace policy

namespace download_dir_util {

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kLocationGoogleDrive[];
extern const char kLocationOneDrive[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
extern const char kDriveNamePolicyVariableName[];
extern const char kOneDriveNamePolicyVariableName[];

// Returns whether |string_value| points to a directory in Drive or not.
bool DownloadToDrive(const base::FilePath::StringType& string_value,
                     const policy::PolicyHandlerParameters& parameters);

// Returns whether |string_value| points to a directory in Microsoft OneDrive or
// not.
bool DownloadToOneDrive(const base::FilePath::StringType& string_value,
                        const policy::PolicyHandlerParameters& parameters);

// Expands the google drive policy variable to the drive root path. This cannot
// be done in ExpandDownloadDirectoryPath() as that gets invoked in a policy
// handler, which are run before the profile is registered.
bool ExpandDrivePolicyVariable(Profile* profile,
                               const base::FilePath& old_path,
                               base::FilePath* new_path);
// Expands the Microsoft OneDrive policy variable to the OneDrive root path.
// This cannot be done in ExpandDownloadDirectoryPath() as that gets invoked in
// a policy handler, which are run before the profile is registered.
bool ExpandOneDrivePolicyVariable(Profile* profile,
                                  const base::FilePath& old_path,
                                  base::FilePath* new_path);

#endif  // BUILDFLAG(IS_CHROMEOS)

// Expands path variables in the download directory path |string_value|.
base::FilePath::StringType ExpandDownloadDirectoryPath(
    const base::FilePath::StringType& string_value,
    const policy::PolicyHandlerParameters& parameters);

}  // namespace download_dir_util

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DIR_UTIL_H_
