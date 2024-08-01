// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_POLICY_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_POLICY_UTILS_H_

#include "base/files/file_path.h"

class Profile;

namespace policy::local_user_files {

extern const char kGoogleDrivePolicyVariableName[],
    kOneDrivePolicyVariableName[];

// Enum describing where the admin configured the files to be saved.
enum class FileSaveDestination {
  kNotSpecified = 0,
  kDownloads = 1,
  kGoogleDrive = 2,
  kOneDrive = 3,
  kMaxValue = kOneDrive,
};

// Supported cloud providers.
enum class CloudProvider {
  kNotSpecified,  // Not set by the policy.
  kGoogleDrive,   // Google Drive.
  kOneDrive,      // Microsoft OneDrive.
};

// Categories of errors that can occur during the file upload process.
enum class MigrationUploadError {
  kServiceUnavailable,  // The cloud provider is not accessible.
  kCopyFailed,          // Copying the file to the destination failed.
  kDeleteFailed,        // Deleting the source file after upload failed.
  kOther,               // An unspecified error occurred.
  kCancelled,           // Upload explicitly cancelled.
};

// Returns whether local user files are enabled on the device by the flag and
// policy.
bool LocalUserFilesAllowed();

// Get the destination where downloads are saved.
FileSaveDestination GetDownloadsDestination(Profile* profile);

// Get the destination where screen captures are saved.
FileSaveDestination GetScreenCaptureDestination(Profile* profile);

// Returns whether `download` should be saved to tmp/ directory.
bool DownloadToTemp(Profile* profile);

// Returns the path of MyFiles folder for `profile`.
base::FilePath GetMyFilesPath(Profile* profile);

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_POLICY_UTILS_H_
