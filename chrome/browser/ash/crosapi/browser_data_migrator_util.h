// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_UTIL_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_UTIL_H_

#include <string>

namespace base {
class FilePath;
}

namespace ash {
namespace browser_data_migrator_util {

constexpr char kTotalSize[] = "Ash.UserDataStatsRecorder.DataSize.TotalSize";

// UMA name prefix to record sizes of files/dirs in profile data directory. The
// name unique to each file/dir is appended to the end to create a full UMA name
// as follows `Ash.UserDataStatsRecorder.DataSize.{ItemName}`.
constexpr char kUserDataStatsRecorderDataSize[] =
    "Ash.UserDataStatsRecorder.DataSize.";

// Files/dirs that is not assigned a unique uma name is given this name.
constexpr char kUnknownUMAName[] = "Unknown";

constexpr int64_t kBytesInOneMB = 1024 * 1024;

// Records `size` of the file/dir pointed by `path`. If it is a directory, the
// size is the recursively accumulated sizes of contents inside.
void RecordUserDataSize(const base::FilePath& path, int64_t size);

// Returns UMA name for `path`. Returns `kUnknownUMAName` if `path` is not in
// `kPathNamePairs`.
std::string GetUMAItemName(const base::FilePath& path);

// Similar to `base::ComputeDirectorySize()` this computes the sum of all files
// under `dir_path` recursively while skipping symlinks.
int64_t ComputeDirectorySizeWithoutLinks(const base::FilePath& dir_path);

// Record the total size of the user's profile data directory in MB.
void RecordTotalSize(int64_t size);

}  // namespace browser_data_migrator_util
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_UTIL_H_
