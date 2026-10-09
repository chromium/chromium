// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_module/child_module_paths.h"

namespace child_module {

base::FilePath CanonicalizeUserDataDir(const base::FilePath& user_data_dir) {
  return user_data_dir;
}

}  // namespace child_module
