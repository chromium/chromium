// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_LOCAL_USER_FILES_LOCAL_FILES_MIGRATION_CONSTANTS_H_
#define CHROME_BROWSER_POLICY_LOCAL_USER_FILES_LOCAL_FILES_MIGRATION_CONSTANTS_H_

#include "base/time/time.h"

namespace policy::local_user_files {

// The total timeout duration for the local files migration process.
constexpr base::TimeDelta kTotalMigrationTimeout = base::Hours(24);

// The final timeout before the migration when an additional dialog is shown.
constexpr base::TimeDelta kFinalMigrationTimeout = base::Hours(1);

// The prefix of the directory the files should be uploaded to. Used with the
// unique identifier of the device to form the directory's full name.
constexpr char kDestinationDirName[] = "ChromeOS device";

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_POLICY_LOCAL_USER_FILES_LOCAL_FILES_MIGRATION_CONSTANTS_H_
