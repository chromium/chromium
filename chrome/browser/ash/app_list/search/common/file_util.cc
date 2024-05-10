// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/file_util.h"

#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {

std::vector<base::FilePath> GetTrashPaths(Profile* profile) {
  std::vector<base::FilePath> excluded_paths;
  if (file_manager::trash::IsTrashEnabledForProfile(profile)) {
    const auto trash_locations =
        file_manager::trash::GenerateEnabledTrashLocationsForProfile(
            profile, /*base_path=*/base::FilePath());
    for (const auto& location : trash_locations) {
      excluded_paths.emplace_back(
          location.first.Append(location.second.relative_folder_path));
    }
  }
  return excluded_paths;
}

}  // namespace app_list
