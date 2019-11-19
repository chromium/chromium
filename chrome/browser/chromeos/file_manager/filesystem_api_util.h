// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The file contains utility functions to implement chrome.fileSystem API for
// file paths that do not directly map to host machine's file system path, such
// as Google Drive or virtual volumes provided by fileSystemProvider extensions.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILESYSTEM_API_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILESYSTEM_API_UTIL_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "storage/common/file_system/file_system_types.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace file_manager {
namespace util {

// Obtains whether |type| is non-native file system or not.
bool IsNonNativeFileSystemType(storage::FileSystemType type);

// Checks whether the given |path| points to a non-local filesystem that
// requires special handling.
bool IsUnderNonNativeLocalPath(Profile* profile, const base::FilePath& path);

// Checks whether |path| points to a filesystem that requires special handling
// for retrieving mime types.
bool HasNonNativeMimeTypeProvider(Profile* profile, const base::FilePath& path);

// Returns the mime type of the file pointed by |path|, and asynchronously sends
// the result to |callback|.
void GetNonNativeLocalPathMimeType(
    Profile* profile,
    const base::FilePath& path,
    base::OnceCallback<void(const base::Optional<std::string>&)> callback);

// Checks whether the |path| points to a directory, and asynchronously sends
// the result to |callback|.
void IsNonNativeLocalPathDirectory(Profile* profile,
                                   const base::FilePath& path,
                                   base::OnceCallback<void(bool)> callback);

// Ensures a file exists at |path|, i.e., it does nothing if a file is already
// present, or creates a file there if it isn't, and asynchronously sends to
// |callback| whether it succeeded.
void PrepareNonNativeLocalFileForWritableApp(
    Profile* profile,
    const base::FilePath& path,
    base::OnceCallback<void(bool)> callback);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILESYSTEM_API_UTIL_H_
