// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_POLICY_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_POLICY_UTILS_H_

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

// Returns whether local user files are enabled on the device by the flag and
// policy.
bool LocalUserFilesAllowed();

// Get the destination where downloads are saved.
FileSaveDestination GetDownloadsDestination(Profile* profile);

// Get the destination where screen captures are saved.
FileSaveDestination GetScreenCaptureDestination(Profile* profile);

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_POLICY_UTILS_H_
