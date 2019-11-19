// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_FILE_UTILS_H_
#define BASE_FUCHSIA_FILE_UTILS_H_

#include <fuchsia/io/cpp/fidl.h>

#include "base/base_export.h"
#include "base/files/file_path.h"

namespace base {
namespace fuchsia {

// Persisted data directory, i.e. /data . Returned as DIR_APP_DATA from
// PathService.
BASE_EXPORT extern const char kPersistedDataDirectoryPath[];

// Services directory, i.e. /svc .
BASE_EXPORT extern const char kServiceDirectoryPath[];

// Package root directory, i.e. /pkg .
BASE_EXPORT extern const char kPackageRootDirectoryPath[];

// Returns fuchsia.io.Directory for the specified |path| or null InterfaceHandle
// if the path doesn't exist or it's not a directory.
BASE_EXPORT fidl::InterfaceHandle<::fuchsia::io::Directory> OpenDirectory(
    const base::FilePath& path);

}  // namespace fuchsia
}  // namespace base

#endif  // BASE_FUCHSIA_FILE_UTILS_H_
