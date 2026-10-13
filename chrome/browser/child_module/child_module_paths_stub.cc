// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_module/child_module_paths.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"

namespace child_module {

// TODO(crbug.com/558598893): Revisit this once we settle on how per-User Data
// dir version tracking is handled, and determine the official staging
// directory for non-Windows platforms (e.g., macOS outer bundle or system
// directories). For now, look under DIR_USER_DATA so development and
// component updater workflows can function.
bool InitializeChildModuleRoot() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    return false;
  }
  return base::PathService::OverrideAndCreateIfNeeded(
      chrome::DIR_CHILD_MODULE_ROOT,
      user_data_dir.Append(kModulesDirName).AppendASCII(chrome::kChromeVersion),
      /*is_absolute=*/true, /*create=*/false);
}

base::FilePath CanonicalizeUserDataDir(const base::FilePath& user_data_dir) {
  return user_data_dir;
}

}  // namespace child_module
