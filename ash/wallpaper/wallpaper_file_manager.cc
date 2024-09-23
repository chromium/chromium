// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_file_manager.h"

#include <string>

#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "ui/gfx/image/image.h"

namespace ash {

namespace {

// Returns the file name of the online wallpaper based on the `resolution`.
std::string GetOnlineWallpaperFileName(const std::string& file_name,
                                       const WallpaperResolution resolution) {
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
base::FilePath GetExistingWallpaperPath(const WallpaperType type,
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

  if (type == WallpaperType::kSeaPen) {
    // SeaPen wallpaper stores WallpaperInfo::location with just the numeric id
    // with no extension. In that case, `ReplaceExtension` will simply append
    // ".jpg". However, other code paths may call this with location="xxx.jpg".
    // `ReplaceExtension` behavior is therefore safer than calling
    // `AddExtension` which may result in ".jpg.jpg".
    wallpaper_path = wallpaper_path.ReplaceExtension(".jpg");
  }

  if (!base::PathExists(wallpaper_path)) {
    return base::FilePath();
  }

  return wallpaper_path;
}

// Deletes the wallpaper directory and its subdirectories to store only the
// latest selected wallpapers. Except online wallpapers for which we want to
// retrieve the wallpapers quickly from cache instead of downloading them again.
bool DeleteWallpaperPath(const WallpaperType type,
                         const base::FilePath& wallpaper_dir) {
  if (IsOnlineWallpaper(type)) {
    return true;
  }
  return base::DeletePathRecursively(wallpaper_dir);
}

// Reads the image from the given `file_path` and returns data as string.
std::string GetStringContent(const base::FilePath& file_path) {
  if (file_path.empty() || !base::PathExists(file_path)) {
    LOG(WARNING) << "File path is empty or does not exist";
    return std::string();
  }

  std::string result;
  if (!base::ReadFileToString(file_path, &result)) {
    LOG(WARNING) << "Failed reading file";
    result.clear();
  }

  return result;
}

base::FilePath GetCustomWallpaperDir(const base::FilePath& wallpaper_dir,
                                     const std::string& sub_dir,
                                     const std::string& wallpaper_files_id) {
  return wallpaper_dir.Append(sub_dir).Append(wallpaper_files_id);
}

// Saves the wallpaper to |path| (absolute path) on filesystem
// and starts resizing operation of the wallpaper if necessary.
// Returns the original path if it is saved successfully.
base::FilePath SaveWallpaperToPath(const WallpaperType type,
                                   const base::FilePath& wallpaper_dir,
                                   const std::string& file_name,
                                   const WallpaperLayout layout,
                                   const gfx::ImageSkia image,
                                   const int resized_width = 0,
                                   const int resized_height = 0) {
  const base::FilePath file_path = wallpaper_dir.Append(file_name);
  if (!DeleteWallpaperPath(type, wallpaper_dir)) {
    LOG(ERROR) << "Failed to delete wallpaper path.";
    return base::FilePath();
  };
  CreateDirectoryAndLogError(wallpaper_dir);
  const bool success = ResizeAndSaveWallpaper(
      image, file_path, layout,
      {resized_width == 0 ? image.width() : resized_width,
       resized_height == 0 ? image.height() : resized_height});
  return success ? file_path : base::FilePath();
}

// Saves the wallpapers into the local file system with different resolution
// sizes based on its wallpaper type.
base::FilePath SaveWallpaperPerType(const WallpaperType type,
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
  }
}

// Reads the image from the given `file_path`. Runs on `blocking_task_runner_`
// thread.
scoped_refptr<base::RefCountedMemory> ReadFile(
    const base::FilePath& file_path) {
  const std::string data = GetStringContent(file_path);
  if (data.empty()) {
    return nullptr;
  }
  return base::MakeRefCounted<base::RefCountedString>(std::move(data));
}

}  // namespace

// This method is thread safe.
base::FilePath GetOnlineWallpaperFilePath(
    const base::FilePath& wallpaper_dir,
    const GURL& url,
    const WallpaperResolution resolution) {
  CHECK(!wallpaper_dir.empty());
  return wallpaper_dir.Append(
      GetOnlineWallpaperFileName(url.ExtractFileName(), resolution));
}

WallpaperFileManager::WallpaperFileManager()
    : blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

WallpaperFileManager::~WallpaperFileManager() = default;

void WallpaperFileManager::LoadWallpaper(const WallpaperType type,
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
    const WallpaperType type,
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
