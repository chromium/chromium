// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_module/child_module_paths.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/win/access_token.h"
#include "base/win/sid.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "chrome/common/chrome_paths.h"

namespace child_module {

namespace {

// Returns the user path component for the current user and the browser's
// User Data directory (`chrome::DIR_USER_DATA`), or an empty path on error.
base::FilePath GetUserPathComponent() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    return base::FilePath();
  }
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  if (!token) {
    return base::FilePath();
  }
  // ComputeUserPathComponent() fails if canonicalization fails.
  return ComputeUserPathComponent(token->User(),
                                  CanonicalizeUserDataDir(user_data_dir));
}

}  // namespace

bool InitializeChildModuleRoot() {
  // GetModulesDirForUser() fails if the user path component is empty.
  const base::FilePath modules_dir =
      GetModulesDirForUser(GetUserPathComponent());
  if (modules_dir.empty()) {
    return false;
  }
  return base::PathService::OverrideAndCreateIfNeeded(
      chrome::DIR_CHILD_MODULE_ROOT, modules_dir, /*is_absolute=*/true,
      /*create=*/false);
}

base::FilePath CanonicalizeUserDataDir(const base::FilePath& user_data_dir) {
  base::FilePath canonical_udd;
  if (user_data_dir.empty() ||
      !base::NormalizeFilePath(user_data_dir, &canonical_udd)) {
    return base::FilePath();
  }
  return canonical_udd;
}

}  // namespace child_module
