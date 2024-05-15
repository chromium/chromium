// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/path_sanitizer.h"

#include "base/notreached.h"
#include "base/path_service.h"

namespace safe_browsing {

PathSanitizer::PathSanitizer() {
  // Get the home directory path.
  if (!base::PathService::Get(base::DIR_HOME, &home_path_))
    NOTREACHED_IN_MIGRATION();
}

const base::FilePath& PathSanitizer::GetHomeDirectory() const {
  return home_path_;
}

void PathSanitizer::StripHomeDirectory(base::FilePath* file_path) const {
  base::FilePath sanitized_path(FILE_PATH_LITERAL("~"));

  // The |file_path| is overwritten only if a relative path is found.
  if (home_path_.AppendRelativePath(*file_path, &sanitized_path))
    *file_path = sanitized_path;
}

}  // namespace safe_browsing
