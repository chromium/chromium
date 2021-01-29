// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_FILE_UTIL_H_
#define BASE_TEST_TEST_FILE_UTIL_H_

// File utility functions used only by tests.

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include <jni.h>
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace base {

// Clear a specific file from the system cache like EvictFileFromSystemCache,
// but on failure it will sleep and retry. On the Windows buildbots, eviction
// can fail if the file is marked in use, and this will throw off timings that
// rely on uncached files.
bool EvictFileFromSystemCacheWithRetry(const FilePath& file);

// Wrapper over base::Delete. On Windows repeatedly invokes Delete in case
// of failure to workaround Windows file locking semantics. Returns true on
// success.
bool DieFileDie(const FilePath& file, bool recurse);

// Creates a a new unique directory and returns the generated path. The
// directory will be automatically deleted when the test completes. Failure
// upon creation or deletion will cause a test failure.
FilePath CreateUniqueTempDirectoryScopedToTest();

// Synchronize all the dirty pages from the page cache to disk (on POSIX
// systems). The Windows analogy for this operation is to 'Flush file buffers'.
// Note: This is currently implemented as a no-op on Windows.
void SyncPageCacheToDisk();

// Clear a specific file from the system cache. After this call, trying
// to access this file will result in a cold load from the hard drive.
bool EvictFileFromSystemCache(const FilePath& file);

#if defined(OS_WIN)
// Deny |permission| on the file |path| for the current user. |permission| is an
// ACCESS_MASK structure which is defined in
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa374892.aspx
// Refer to https://msdn.microsoft.com/en-us/library/aa822867.aspx for a list of
// possible values.
bool DenyFilePermission(const FilePath& path, DWORD permission);
#endif  // defined(OS_WIN)

// For testing, make the file unreadable or unwritable.
// In POSIX, this does not apply to the root user.
bool MakeFileUnreadable(const FilePath& path) WARN_UNUSED_RESULT;
bool MakeFileUnwritable(const FilePath& path) WARN_UNUSED_RESULT;

// Saves the current permissions for a path, and restores it on destruction.
class FilePermissionRestorer {
 public:
  explicit FilePermissionRestorer(const FilePath& path);
  ~FilePermissionRestorer();

 private:
  const FilePath path_;
  void* info_;  // The opaque stored permission information.
  size_t length_;  // The length of the stored permission information.

  DISALLOW_COPY_AND_ASSIGN(FilePermissionRestorer);
};

#if defined(OS_ANDROID)
// Insert an image file into the MediaStore, and retrieve the content URI for
// testing purpose.
FilePath InsertImageIntoMediaStore(const FilePath& path);
#endif  // defined(OS_ANDROID)

}  // namespace base

#endif  // BASE_TEST_TEST_FILE_UTIL_H_
