// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper/wallpaper_enumerator.h"

#include <string>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

constexpr char kPngFilePattern[] = "*.[pP][nN][gG]";
constexpr char kJpgFilePattern[] = "*.[jJ][pP][gG]";
constexpr char kJpegFilePattern[] = "*.[jJ][pP][eE][gG]";

constexpr int kMaximumImageCount = 1000;

void EnumerateFiles(const base::FilePath& path,
                    const std::vector<base::FilePath>& trash_paths,
                    const std::string& pattern,
                    std::vector<base::FilePath>* out) {
  base::FileEnumerator image_enumerator(
      path, /*recursive=*/true, base::FileEnumerator::FILES,
      FILE_PATH_LITERAL(pattern),
      base::FileEnumerator::FolderSearchPolicy::ALL);

  for (base::FilePath image_path = image_enumerator.Next();
       !image_path.empty() && out->size() < kMaximumImageCount;
       image_path = image_enumerator.Next()) {
    if (base::ranges::any_of(
            trash_paths, [&image_path](const base::FilePath& trash_path) {
              // Equivalent to
              // image_path.value().starts_with(trash_path.value()).
              return image_path.value().rfind(trash_path.value(), 0) == 0;
            })) {
      continue;
    }
    out->push_back(image_path);
  }
}

// Looks up all the images in the |search_path| and excludes the ones that
// overlapse with |trash_paths|.
std::vector<base::FilePath> EnumerateAllImages(
    const base::FilePath& search_path,
    const std::vector<base::FilePath>& trash_paths) {
  std::vector<base::FilePath> image_paths;

  EnumerateFiles(search_path, trash_paths, kPngFilePattern, &image_paths);
  EnumerateFiles(search_path, trash_paths, kJpgFilePattern, &image_paths);
  EnumerateFiles(search_path, trash_paths, kJpegFilePattern, &image_paths);

  return image_paths;
}

}  // namespace

namespace ash {

void EnumerateLocalWallpaperFiles(
    Profile* profile,
    base::OnceCallback<void(const std::vector<base::FilePath>&)> callback) {
  const base::FilePath search_path =
      file_manager::util::GetMyFilesFolderForProfile(profile);

  std::vector<base::FilePath> trash_paths;
  if (file_manager::trash::IsTrashEnabledForProfile(profile)) {
    auto enabled_trash_locations =
        file_manager::trash::GenerateEnabledTrashLocationsForProfile(
            profile, /*base_path=*/base::FilePath());
    for (const auto& it : enabled_trash_locations) {
      base::FilePath trash_path =
          it.first.Append(it.second.relative_folder_path);
      trash_paths.emplace_back(trash_path);
    }
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&EnumerateAllImages, search_path, trash_paths),
      std::move(callback));
}

}  // namespace ash
