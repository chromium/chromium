// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_module/child_module_paths.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"

namespace child_module {

base::FilePath CanonicalizeUserDataDir(const base::FilePath& user_data_dir) {
  base::FilePath canonical_udd;
  if (user_data_dir.empty() ||
      !base::NormalizeFilePath(user_data_dir, &canonical_udd)) {
    return base::FilePath();
  }
  return canonical_udd;
}

}  // namespace child_module
