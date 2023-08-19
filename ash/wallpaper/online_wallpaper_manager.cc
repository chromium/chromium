// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/online_wallpaper_manager.h"

#include <memory>

#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_image_downloader.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resolution.h"
#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

namespace ash {

namespace {

// *****************************************************************************
// File Operations
// *****************************************************************************

// Returns the file path of the wallpaper corresponding to `url` and the
// `resolution` from `wallpaper_dir` if it exists in local file system,
// otherwise returns an empty file path. Runs on `sequenced_task_runner_`
// thread.
base::FilePath GetExistingOnlineWallpaperPath(
    const base::FilePath& wallpaper_dir,
    const GURL& url,
    WallpaperResolution resolution) {
  base::FilePath wallpaper_path =
      GetOnlineWallpaperPath(wallpaper_dir, url, resolution);
  if (base::PathExists(wallpaper_path)) {
    return wallpaper_path;
  }

  // Falls back to the large wallpaper if the small one doesn't exist.
  if (resolution == WallpaperResolution::kSmall) {
    wallpaper_path =
        GetOnlineWallpaperPath(wallpaper_dir, url, WallpaperResolution::kLarge);
    if (base::PathExists(wallpaper_path)) {
      return wallpaper_path;
    }
  }
  return base::FilePath();
}

// Saves the online wallpaper with both large and small sizes to local file
// system. Runs on `sequenced_task_runner_` thread.
void SaveToDiskBlocking(const base::FilePath& wallpaper_dir,
                        const GURL& url,
                        WallpaperLayout layout,
                        const gfx::ImageSkia& image) {
  if (!base::DirectoryExists(wallpaper_dir) &&
      !base::CreateDirectory(wallpaper_dir)) {
    LOG(ERROR) << "Failed to create directory for online wallpaper: "
               << wallpaper_dir;
    return;
  }
  ResizeAndSaveWallpaper(
      image,
      GetOnlineWallpaperPath(wallpaper_dir, url, WallpaperResolution::kLarge),
      layout, image.width(), image.height());
  ResizeAndSaveWallpaper(
      image,
      GetOnlineWallpaperPath(wallpaper_dir, url, WallpaperResolution::kSmall),
      WALLPAPER_LAYOUT_CENTER_CROPPED, kSmallWallpaperMaxWidth,
      kSmallWallpaperMaxHeight);
}

// Reads the image from the given `file_path`. Runs on `sequenced_task_runner_`
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
base::FilePath GetOnlineWallpaperPath(const base::FilePath& wallpaper_dir,
                                      const GURL& url,
                                      WallpaperResolution resolution) {
  std::string file_name = url.ExtractFileName();
  if (resolution == WallpaperResolution::kSmall) {
    file_name =
        base::FilePath(file_name)
            .InsertBeforeExtension(wallpaper_constants::kSmallWallpaperSuffix)
            .value();
  }
  DCHECK(!wallpaper_dir.empty());
  return wallpaper_dir.Append(file_name);
}

OnlineWallpaperManager::OnlineWallpaperManager(
    WallpaperImageDownloader* wallpaper_image_downloader)
    : wallpaper_image_downloader_(wallpaper_image_downloader),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

OnlineWallpaperManager::~OnlineWallpaperManager() = default;

void OnlineWallpaperManager::LoadOnlineWallpaper(
    const base::FilePath& wallpaper_dir,
    const GURL& url,
    LoadOnlineWallpaperCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WallpaperResolution resolution = GetAppropriateResolution();
  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetExistingOnlineWallpaperPath, wallpaper_dir, url,
                     resolution),
      base::BindOnce(&OnlineWallpaperManager::LoadFromDisk,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void OnlineWallpaperManager::DownloadAndSaveOnlineWallpaper(
    const base::FilePath& wallpaper_dir,
    const OnlineWallpaperParams& params,
    LoadOnlineWallpaperCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto on_load = base::BindOnce(
      &OnlineWallpaperManager::OnLoadExistingOnlineWallpaperComplete,
      weak_factory_.GetWeakPtr(), wallpaper_dir, params, std::move(callback));
  LoadOnlineWallpaper(wallpaper_dir, params.url, std::move(on_load));
}

void OnlineWallpaperManager::LoadOnlineWallpaperPreview(
    const base::FilePath& wallpaper_dir,
    const GURL& preview_url,
    LoadPreviewImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath preview_image_path = GetOnlineWallpaperPath(
      wallpaper_dir, preview_url, GetAppropriateResolution());
  // Uses `sequenced_task_runner_` to ensure that the wallpaper is saved
  // successfully before one of its variants is used as the preview image.
  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadFile, preview_image_path),
      std::move(callback));
}

void OnlineWallpaperManager::LoadFromDisk(LoadOnlineWallpaperCallback callback,
                                          const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool file_exists = !file_path.empty();
  if (!file_exists) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  image_util::DecodeImageFile(std::move(callback), file_path);
}

void OnlineWallpaperManager::OnLoadExistingOnlineWallpaperComplete(
    const base::FilePath& wallpaper_dir,
    const OnlineWallpaperParams& params,
    LoadOnlineWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  if (image.isNull()) {
    DownloadAndSaveAllVariants(wallpaper_dir, params, std::move(callback));
  } else {
    std::move(callback).Run(image);
  }
}

void OnlineWallpaperManager::DownloadAndSaveAllVariants(
    const base::FilePath& wallpaper_dir,
    const OnlineWallpaperParams& params,
    LoadOnlineWallpaperCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<OnlineWallpaperVariant> variants = params.variants;
  if (variants.empty()) {
    // `variants` can be empty for users who have just migrated from the old
    // wallpaper picker to the new one.
    //
    // OnlineWallpaperVariant's `asset_id` and `type` are not actually used in
    // this function, so they can have dummy values here.
    variants.emplace_back(/*asset_id=*/0, params.url,
                          backdrop::Image::IMAGE_TYPE_UNKNOWN);
  }

  // There's only one variant that is actually needed to fulfill the immediate
  // request. However, it's important that all of the other variants are
  // available as well (ex: the user picks a wallpaper and toggles between D/L
  // modes to see what it looks like). As such, the whole operation is
  // considered a failure unless all variants are downloaded (otherwise the
  // feature is confusing as it would advertise multiple variants but only
  // have one).
  auto downloads_result = std::make_unique<VariantsDownloadResult>();
  auto* downloads_result_ptr = downloads_result.get();
  auto on_all_variants_downloaded = base::BarrierClosure(
      variants.size(),
      base::BindOnce(&OnlineWallpaperManager::OnAllVariantsDownloaded,
                     weak_factory_.GetWeakPtr(), std::move(downloads_result),
                     std::move(callback)));
  for (const OnlineWallpaperVariant& variant : variants) {
    wallpaper_image_downloader_->DownloadBackdropImage(
        variant.raw_url, params.account_id,
        base::BindOnce(
            &OnlineWallpaperManager::OnVariantDownloaded,
            weak_factory_.GetWeakPtr(), wallpaper_dir, variant.raw_url,
            params.layout, /*is_target_variant=*/params.url == variant.raw_url,
            // Since `downloads_result`'s lifetime matches the
            // OnAllVariantsDownloaded() callback, and OnAllVariantsDownloaded()
            // is guaranteed to be run after OnVariantDownloaded(), there's no
            // possibility of use-after-free.
            base::Unretained(downloads_result_ptr),
            on_all_variants_downloaded));
  }
}

void OnlineWallpaperManager::OnAllVariantsDownloaded(
    std::unique_ptr<VariantsDownloadResult> downloads_result,
    LoadOnlineWallpaperCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(downloads_result);
  // Due to the order of variants being downloaded, `target_variant` may have
  // been set when others fail to download. To ensure it's all or nothing, we
  // need to reset it so the operation is considered a failure.
  if (downloads_result->any_downloads_failed) {
    downloads_result->target_variant = gfx::ImageSkia();
  }
  std::move(callback).Run(std::move(downloads_result->target_variant));
}

void OnlineWallpaperManager::OnVariantDownloaded(
    const base::FilePath& wallpaper_dir,
    const GURL& variant_url,
    WallpaperLayout layout,
    bool is_target_variant,
    VariantsDownloadResult* downloads_result,
    base::RepeatingClosure on_done,
    const gfx::ImageSkia& image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(downloads_result);
  if (image.isNull()) {
    LOG(WARNING) << "Image download failed";
    downloads_result->any_downloads_failed = true;
    std::move(on_done).Run();
    return;
  }

  gfx::ImageSkia image_to_save = image;
  if (is_target_variant) {
    image.EnsureRepsForSupportedScales();
    // If this is the target variant, the `image` will get passed back to the
    // caller, who may mutate the underlying image's memory somehow.
    // SaveToDiskBlocking() may also mutate the image before saving it, so it
    // would be problematic if both operated on the same underlying image
    // memory. To completely avoid this, pass a separate deep copy to
    // SaveToDiskBlocking().
    image_to_save = image.DeepCopy();
    downloads_result->target_variant = image;
  }

  // Posts a task of saving the image to disk via `sequenced_task_runner_` to
  // ensure the operation order is maintain. It is important this task is
  // executed before loading the preview image to make sure the files have been
  // saved on disk.
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SaveToDiskBlocking, wallpaper_dir, variant_url,
                                layout, std::move(image_to_save)));
  std::move(on_done).Run();
}

}  // namespace ash
