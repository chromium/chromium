// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_file_manager.h"

#include "ash/public/cpp/image_util.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "ui/gfx/image/image.h"

namespace ash {

namespace {
// The max number of Sea Pen image files to keep in Sea Pen directory before
// adding a new file.
constexpr int kMaxSeaPenFiles = 9;

// Returns the file name of the online wallpaper based on the `resolution`.
std::string GetOnlineWallpaperFileName(const std::string& file_name,
                                       WallpaperResolution resolution) {
  if (resolution == WallpaperResolution::kSmall) {
    return base::FilePath(file_name)
        .InsertBeforeExtension(wallpaper_constants::kSmallWallpaperSuffix)
        .value();
  }
  return file_name;
}

// Returns the file path of the wallpaper corresponding to wallpaper location
// info and the from `wallpaper_dir` if it exists in local file system,
// otherwise returns an empty file path. Runs on `blocking_task_runner_`
// thread.
base::FilePath GetExistingWallpaperPath(WallpaperType type,
                                        const base::FilePath& wallpaper_dir,
                                        const std::string& location) {
  base::FilePath wallpaper_path;
  // If the wallpaper is an online wallpaper, its location info is the image
  // url. If it is a Google Photos wallpaper, its location is the image name.
  if (IsOnlineWallpaper(type)) {
    wallpaper_path = GetOnlineWallpaperFilePath(wallpaper_dir, GURL(location),
                                                GetAppropriateResolution());
    if (base::PathExists(wallpaper_path)) {
      return wallpaper_path;
    }

    wallpaper_path = GetOnlineWallpaperFilePath(wallpaper_dir, GURL(location),
                                                WallpaperResolution::kLarge);
    if (base::PathExists(wallpaper_path)) {
      return wallpaper_path;
    }
  }

  wallpaper_path = wallpaper_dir.Append(location);
  if (base::PathExists(wallpaper_path)) {
    return wallpaper_path;
  }

  return base::FilePath();
}

// Creates the wallpaper directory in the local file system for caching
// the wallpapers if it does not already exist.
void EnsureWallpaperDirectoryExists(const base::FilePath& wallpaper_dir) {
  if (!base::DirectoryExists(wallpaper_dir)) {
    base::CreateDirectory(wallpaper_dir);
  }
}

// Scans through all the images in Sea Pen wallpaper directory. Keeps only 9
// latest sea pen images based on the last modified time, the older files are
// removed. Returns true if the process is successful.
bool MaybeDeleteOldSeaPenImages(base::FilePath wallpaper_dir) {
  std::vector<std::pair<base::FilePath, base::Time>> sea_pen_files;

  // Enumerate normal files only; directories and symlinks are skipped.
  base::FileEnumerator enumerator(wallpaper_dir, true,
                                  base::FileEnumerator::FILES);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    DCHECK_EQ(".jpg", file_path.Extension());
    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();
    sea_pen_files.emplace_back(file_path, info.GetLastModifiedTime());
  }

  if (sea_pen_files.size() <= kMaxSeaPenFiles) {
    return true;
  }

  // Finds the n oldest files (n = total files - kMaxSeaPenFiles) then resizes
  // sea_pen_files to store only the old files.
  std::nth_element(sea_pen_files.begin(), sea_pen_files.end() - kMaxSeaPenFiles,
                   sea_pen_files.end(),
                   [](const auto& left, const auto& right) {
                     return left.second < right.second;
                   });
  sea_pen_files.resize(sea_pen_files.size() - kMaxSeaPenFiles);

  // Removes all the old images.
  for (const auto& [file_path, _] : sea_pen_files) {
    if (!base::DeleteFile(file_path)) {
      LOG(ERROR) << __func__ << " failed to remove old Sea Pen file";
      return false;
    }
  }

  return true;
}

// Deletes the wallpaper directory and its subdirectories to store only the
// latest selected wallpapers. Except online wallpapers for which we want to
// retrieve the wallpapers quickly from cache instead of downloading them again.
bool DeleteWallpaperPath(WallpaperType type,
                         const base::FilePath& wallpaper_dir) {
  if (IsOnlineWallpaper(type)) {
    return true;
  }
  if (type == WallpaperType::kSeaPen) {
    return MaybeDeleteOldSeaPenImages(wallpaper_dir);
  }
  return base::DeletePathRecursively(wallpaper_dir);
}

base::FilePath GetCustomWallpaperDir(const base::FilePath& wallpaper_dir,
                                     const std::string& sub_dir,
                                     const std::string& wallpaper_files_id) {
  return wallpaper_dir.Append(sub_dir).Append(wallpaper_files_id);
}

// Saves the wallpaper to |path| (absolute path) on filesystem
// and starts resizing operation of the wallpaper if necessary.
// Returns the original path if it is saved successfully.
base::FilePath SaveWallpaperToPath(WallpaperType type,
                                   const base::FilePath& wallpaper_dir,
                                   const std::string& file_name,
                                   WallpaperLayout layout,
                                   gfx::ImageSkia image,
                                   int resized_width = 0,
                                   int resized_height = 0) {
  const base::FilePath file_path = wallpaper_dir.Append(file_name);
  if (!DeleteWallpaperPath(type, wallpaper_dir)) {
    LOG(ERROR) << "Failed to delete wallpaper path.";
    return base::FilePath();
  };
  EnsureWallpaperDirectoryExists(wallpaper_dir);
  const bool success = ResizeAndSaveWallpaper(
      image, file_path, layout, resized_width ? resized_width : image.width(),
      resized_height ? resized_height : image.height());
  return success ? file_path : base::FilePath();
}

// Saves the wallpapers into the local file system with different resolution
// sizes based on its wallpaper type.
base::FilePath SaveWallpaperPerType(WallpaperType type,
                                    const base::FilePath& wallpaper_dir,
                                    const std::string& wallpaper_files_id,
                                    const std::string& file_name,
                                    const WallpaperLayout layout,
                                    const gfx::ImageSkia& image) {
  switch (type) {
    case WallpaperType::kOnline:
    case WallpaperType::kDaily: {
      // Saves the online wallpaper with both small and large sizes to local
      // file system.
      const std::string small_wallpaper_file_name =
          GetOnlineWallpaperFileName(file_name, WallpaperResolution::kSmall);
      SaveWallpaperToPath(type, wallpaper_dir, small_wallpaper_file_name,
                          WALLPAPER_LAYOUT_CENTER_CROPPED, image,
                          kSmallWallpaperMaxWidth, kSmallWallpaperMaxHeight);
      return SaveWallpaperToPath(type, wallpaper_dir, file_name, layout, image);
    }
    case WallpaperType::kCustomized:
    case WallpaperType::kPolicy: {
      // Save the custom wallpaper with small, large and original sizes to the
      // local file system.
      CHECK(!wallpaper_files_id.empty());
      SaveWallpaperToPath(
          type,
          GetCustomWallpaperDir(wallpaper_dir, kSmallWallpaperSubDir,
                                wallpaper_files_id),
          file_name, layout, image, kSmallWallpaperMaxWidth,
          kSmallWallpaperMaxHeight);
      SaveWallpaperToPath(
          type,
          GetCustomWallpaperDir(wallpaper_dir, kLargeWallpaperSubDir,
                                wallpaper_files_id),
          file_name, layout, image, kLargeWallpaperMaxWidth,
          kLargeWallpaperMaxHeight);
      return SaveWallpaperToPath(
          type,
          GetCustomWallpaperDir(wallpaper_dir, kOriginalWallpaperSubDir,
                                wallpaper_files_id),
          file_name, WALLPAPER_LAYOUT_STRETCH, image);
    }
    case WallpaperType::kOnceGooglePhotos:
    case WallpaperType::kDailyGooglePhotos:
    case WallpaperType::kSeaPen:
      // Save the Google Photo and Sea Pen wallpaper in original size to the
      // local file system.
      return SaveWallpaperToPath(type, wallpaper_dir, file_name, layout, image);
    default:
      NOTREACHED() << "Invalid wallpaper type.";
      return base::FilePath();
  }
}

// Reads the image from the given `file_path`. Runs on `blocking_task_runner_`
// thread.
scoped_refptr<base::RefCountedMemory> ReadFile(
    const base::FilePath& file_path) {
  if (file_path.empty()) {
    return nullptr;
  }

  std::string data;
  if (!base::ReadFileToString(file_path, &data)) {
    return nullptr;
  }
  return base::MakeRefCounted<base::RefCountedString>(std::move(data));
}

}  // namespace

// This method is thread safe.
base::FilePath GetOnlineWallpaperFilePath(const base::FilePath& wallpaper_dir,
                                          const GURL& url,
                                          WallpaperResolution resolution) {
  CHECK(!wallpaper_dir.empty());
  return wallpaper_dir.Append(
      GetOnlineWallpaperFileName(url.ExtractFileName(), resolution));
}

WallpaperFileManager::WallpaperFileManager()
    : blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

WallpaperFileManager::~WallpaperFileManager() = default;

void WallpaperFileManager::LoadWallpaper(WallpaperType type,
                                         const base::FilePath& wallpaper_dir,
                                         const std::string& location,
                                         LoadWallpaperCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetExistingWallpaperPath, type, wallpaper_dir, location),
      base::BindOnce(&WallpaperFileManager::LoadFromDisk,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WallpaperFileManager::LoadOnlineWallpaperPreview(
    const base::FilePath& wallpaper_dir,
    const GURL& url,
    LoadPreviewImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Uses `blocking_task_runner_` to ensure that the wallpaper is saved
  // successfully before one of its variants is used as the preview image.
  const base::FilePath& preview_image_path = GetOnlineWallpaperFilePath(
      wallpaper_dir, url, GetAppropriateResolution());
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadFile, preview_image_path),
      std::move(callback));
}

void WallpaperFileManager::SaveWallpaperToDisk(
    WallpaperType type,
    const base::FilePath& wallpaper_dir,
    const std::string& file_name,
    const WallpaperLayout layout,
    const gfx::ImageSkia& image,
    SaveWallpaperCallback callback,
    const std::string& wallpaper_files_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (image.isNull()) {
    std::move(callback).Run(base::FilePath());
    return;
  }

  image.EnsureRepsForSupportedScales();
  gfx::ImageSkia deep_copy(image.DeepCopy());
  // Block shutdown on this task. Otherwise, we may lose the wallpaper that the
  // user selected.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SaveWallpaperPerType, type, wallpaper_dir,
                     wallpaper_files_id, file_name, layout, deep_copy),
      std::move(callback));
}

void WallpaperFileManager::LoadFromDisk(LoadWallpaperCallback callback,
                                        const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (file_path.empty()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  image_util::DecodeImageFile(std::move(callback), file_path);
}

}  // namespace ash
