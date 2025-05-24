// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_LOCAL_USER_FILES_LOCAL_FILES_MIGRATION_CONSTANTS_H_
#define CHROME_BROWSER_POLICY_LOCAL_USER_FILES_LOCAL_FILES_MIGRATION_CONSTANTS_H_

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace policy::local_user_files {

// The total timeout duration for the local files migration process.
inline constexpr base::TimeDelta kTotalMigrationTimeout = base::Hours(24);

// The final timeout before the migration when an additional dialog is shown.
inline constexpr base::TimeDelta kFinalMigrationTimeout = base::Hours(1);

// The prefix of the directory the files should be uploaded to. Used with the
// unique identifier of the device to form the directory's full name.
inline constexpr char kUploadRootPrefix[] = "ChromeOS device";

// The maximum number of retries before failing the migration.
inline constexpr int kMaxRetryCount = 20;

// The path where the log file for migration upload errors is stored.
inline constexpr base::FilePath::CharType kErrorLogFileBasePath[] =
    FILE_PATH_LITERAL("/home/chronos/user/log/");

// The name of the log file for migration upload errors.
inline constexpr base::FilePath::CharType kErrorLogFileName[] =
    FILE_PATH_LITERAL("local_files_upload");

// The amount of time a file uploader waits for network reconnection before
// failing.
inline constexpr base::TimeDelta kReconnectionTimeout = base::Hours(4);

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_POLICY_LOCAL_USER_FILES_LOCAL_FILES_MIGRATION_CONSTANTS_H_
