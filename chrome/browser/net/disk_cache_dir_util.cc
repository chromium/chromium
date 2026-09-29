// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/disk_cache_dir_util.h"

#include "base/files/file_path.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chrome_browser_net {

base::FilePath GetDiskCacheDir(const PrefService* local_state) {
  if (!local_state) {
    return base::FilePath();
  }
  const PrefService::Preference* pref =
      local_state->FindPreference(prefs::kDiskCacheDir);
  if (!pref || pref->IsUserControlled()) {
    return base::FilePath();
  }

  const base::FilePath pref_cache_dir =
      local_state->GetFilePath(prefs::kDiskCacheDir);
  if (pref_cache_dir.empty() || !pref_cache_dir.IsAbsolute() ||
      pref_cache_dir.ReferencesParent()) {
    return base::FilePath();
  }

  return pref_cache_dir.StripTrailingSeparators();
}

}  // namespace chrome_browser_net
