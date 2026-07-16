// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/file_util.h"

#include "base/check_deref.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {

std::vector<base::FilePath> GetTrashPaths(Profile* profile) {
  std::vector<base::FilePath> excluded_paths;
  // TODO(crbug.com/404129453): Avoid using g_browser_process.
  if (file_manager::trash::IsTrashEnabledForProfile(
          CHECK_DEREF(g_browser_process->local_state()), profile)) {
    const auto trash_locations =
        file_manager::trash::GenerateEnabledTrashLocationsForProfile(profile);
    for (const auto& location : trash_locations) {
      excluded_paths.emplace_back(
          location.first.Append(location.second.relative_folder_path));
    }
  }
  return excluded_paths;
}

}  // namespace app_list
