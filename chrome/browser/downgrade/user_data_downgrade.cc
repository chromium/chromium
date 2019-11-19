// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/user_data_downgrade.h"

#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace downgrade {

const base::FilePath::StringPieceType kDowngradeLastVersionFile(
    FILE_PATH_LITERAL("Last Version"));
const base::FilePath::StringPieceType kDowngradeDeleteSuffix(
    FILE_PATH_LITERAL(".CHROME_DELETE"));

base::FilePath GetLastVersionFile(const base::FilePath& user_data_dir) {
  DCHECK(!user_data_dir.empty());
  return user_data_dir.Append(kDowngradeLastVersionFile);
}

base::Optional<base::Version> GetLastVersion(
    const base::FilePath& user_data_dir) {
  DCHECK(!user_data_dir.empty());
  std::string last_version_str;
  if (base::ReadFileToString(GetLastVersionFile(user_data_dir),
                             &last_version_str)) {
    base::Version version(
        base::TrimWhitespaceASCII(last_version_str, base::TRIM_ALL));
    if (version.IsValid())
      return version;
  }
  return base::nullopt;
}

base::FilePath GetDiskCacheDir() {
  base::FilePath disk_cache_dir =
      g_browser_process->local_state()->GetFilePath(prefs::kDiskCacheDir);
  if (disk_cache_dir.ReferencesParent())
    return base::MakeAbsoluteFilePath(disk_cache_dir);
  return disk_cache_dir;
}

}  // namespace downgrade
