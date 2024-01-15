// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNGRADE_USER_DATA_DOWNGRADE_H_
#define CHROME_BROWSER_DOWNGRADE_USER_DATA_DOWNGRADE_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "base/version.h"

namespace base {
class Time;
}

namespace downgrade {

// The suffix of pending deleted directory.
extern const base::FilePath::StringPieceType kDowngradeDeleteSuffix;

// The name of "Last Version" file.
extern const base::FilePath::StringPieceType kDowngradeLastVersionFile;

// The name of the Snapshot directory.
extern const base::FilePath::StringPieceType kSnapshotsDir;

// Returns the path to the "Last Version" file in |user_data_dir|.
base::FilePath GetLastVersionFile(const base::FilePath& user_data_dir);

// Returns the value contained in the "Last Version" file in |user_data_dir|, or
// a null value if the file does not exist, cannot be read, or does not contain
// a version number.
std::optional<base::Version> GetLastVersion(
    const base::FilePath& user_data_dir);

// Return the disk cache directory override if one is set via administrative
// policy or a command line switch; otherwise, an empty path (the disk cache is
// within the User Data directory).
base::FilePath GetDiskCacheDir();

// Returns the versions that have a complete snapshot available.
base::flat_set<base::Version> GetAvailableSnapshots(
    const base::FilePath& snapshot_dir);

// Returns the absolute path of directories under |snapshot_dir| that are
// incomplete snapshots or badly named.
std::vector<base::FilePath> GetInvalidSnapshots(
    const base::FilePath& snapshot_dir);

// Return the highest available snapshot version that is not greater than
// |version|.
std::optional<base::Version> GetSnapshotToRestore(
    const base::Version& version,
    const base::FilePath& user_data_dir);

// Removes snapshot data created after |delete_begin| for |profile_path|.
// |remove_mask| (of bits from ChromeBrowsingDataRemoverDelegate::DataType)
// indicates the types of data to be cleared from the profile's snapshots.
void RemoveDataForProfile(base::Time delete_begin,
                          const base::FilePath& profile_path,
                          uint64_t remove_mask);

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_USER_DATA_DOWNGRADE_H_
