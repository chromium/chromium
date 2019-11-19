// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNGRADE_USER_DATA_DOWNGRADE_H_
#define CHROME_BROWSER_DOWNGRADE_USER_DATA_DOWNGRADE_H_

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/version.h"

namespace downgrade {

// The suffix of pending deleted directory.
extern const base::FilePath::StringPieceType kDowngradeDeleteSuffix;

// The name of "Last Version" file.
extern const base::FilePath::StringPieceType kDowngradeLastVersionFile;

// Returns the path to the "Last Version" file in |user_data_dir|.
base::FilePath GetLastVersionFile(const base::FilePath& user_data_dir);

// Returns the value contained in the "Last Version" file in |user_data_dir|, or
// a null value if the file does not exist, cannot be read, or does not contain
// a version number.
base::Optional<base::Version> GetLastVersion(
    const base::FilePath& user_data_dir);

// Return the disk cache directory override if one is set via administrative
// policy or a command line switch; otherwise, an empty path (the disk cache is
// within the User Data directory).
base::FilePath GetDiskCacheDir();

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_USER_DATA_DOWNGRADE_H_
