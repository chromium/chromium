// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_PATHS_H_
#define CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_PATHS_H_

#include "base/files/file_path.h"

namespace child_module {

// Determines the directory in which child module versions for the running
// browser are staged and sets `chrome::DIR_CHILD_MODULE_ROOT` to it. Returns
// false, leaving the key unchanged, on failure. The directory is:
//   - Windows: <InstallDir>/<BaseVersion>/ChildModules/<UserPathComponent>,
//     where UserPathComponent identifies the current user and the browser's
//     User Data directory (see `ComputeUserPathComponent()`).
//   - Other platforms: <DIR_USER_DATA>/ChildModules/<BaseVersion>
// The directory need not exist. This accesses the filesystem, and is called
// once by the browser process during startup.
bool InitializeChildModuleRoot();

// Returns the canonical form of `user_data_dir` for use with
// `ComputeUserPathComponent()`, or an empty path if it cannot be determined
// (e.g., the directory does not exist or its canonical path is too long). This
// call performs file I/O.
base::FilePath CanonicalizeUserDataDir(const base::FilePath& user_data_dir);

}  // namespace child_module

#endif  // CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_PATHS_H_
