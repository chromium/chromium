// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_FILE_UTILS_H_
#define BASE_FUCHSIA_FILE_UTILS_H_

#include <fuchsia/io/cpp/fidl.h>

#include "base/base_export.h"
#include "base/files/file_path.h"

namespace base {

// Persisted data directory, i.e. /data .
BASE_EXPORT extern const char kPersistedDataDirectoryPath[];

// Persisted cache directory, i.e. /cache .
BASE_EXPORT extern const char kPersistedCacheDirectoryPath[];

// Services directory, i.e. /svc .
BASE_EXPORT extern const char kServiceDirectoryPath[];

// Package root directory, i.e. /pkg .
BASE_EXPORT extern const char kPackageRootDirectoryPath[];

// Describes the level of access requested.
//
// See fuchsia.io/OpenFlags and fuchsia.io/Directory.Open for details.
//
// Note that rights are hierarchical in Fuchsia filesystems, so this level of
// access applies to the directory connection itself as well as any connections
// derived from it using fuchsia.io/Directory.Open.
struct DirectoryHandleRights {
  // fuchsia.io/OpenFlags.RIGHT_READABLE.
  bool readable = false;
  // fuchsia.io/OpenFlags.RIGHT_WRITABLE.
  bool writable = false;
  // fuchsia.io/OpenFlags.RIGHT_EXECUTABLE.
  bool executable = false;
};

// Returns a fuchsia.io/Directory for the specified |path|, or an
// invalid InterfaceHandle if the path doesn't exist or it's not a directory.
BASE_EXPORT fidl::InterfaceHandle<::fuchsia::io::Directory> OpenDirectoryHandle(
    const base::FilePath& path,
    DirectoryHandleRights rights);

}  // namespace base

#endif  // BASE_FUCHSIA_FILE_UTILS_H_
