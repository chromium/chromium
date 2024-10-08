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

// The event or action that triggers an upload to the cloud.
enum class UploadTrigger {
  kDownload = 0,
  kScreenCapture = 1,
  kMigration = 2,
  kMaxValue = kMigration,
};

// Possible states of the migration. Persisted to a pref.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(State)
enum class State {
  kUninitialized = 0,
  kPending = 1,
  kInProgress = 2,
  kCleanup = 3,
  kCompleted = 4,
  kFailure = 5,
  kMaxValue = kFailure,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:EnterpriseSkyVaultMigrationState)

// The context, or the part of the migration process in which an unexpected
// state transition happens.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(StateErrorContext)
enum class StateErrorContext {
  kShowDialog = 0,  //
  kDialogClick = 1,
  kSkipTimeout = 2,
  kTimeout = 3,
  kListFiles = 4,
  kMigrationStart = 5,
  kMigrationDone = 6,
  kCleanupStart = 7,
  kCleanupDone = 8,
  kMaxValue = kCleanupDone,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:EnterpriseSkyVaultMigrationStateErrorContext)

// Returns whether local user files are enabled on the device by the flag and
// policy.
bool LocalUserFilesAllowed();

// If SkyVault migration is enabled, returns the `CloudProvider` to which local
// files should be uploaded, and `kNotSpecified` otherwise.
CloudProvider GetMigrationDestination();

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
