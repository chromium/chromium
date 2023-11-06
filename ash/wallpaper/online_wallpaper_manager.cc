// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/online_wallpaper_manager.h"

#include "ash/public/cpp/image_util.h"
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
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

namespace ash {

OnlineWallpaperManager::OnlineWallpaperManager(
    WallpaperImageDownloader* wallpaper_image_downloader,
    WallpaperFileManager* wallpaper_file_manager)
    : wallpaper_image_downloader_(wallpaper_image_downloader),
      wallpaper_file_manager_(wallpaper_file_manager) {}

OnlineWallpaperManager::~OnlineWallpaperManager() = default;

void OnlineWallpaperManager::GetOnlineWallpaper(
    const base::FilePath& wallpaper_dir,
    const AccountId& account_id,
    const WallpaperInfo& wallpaper_info,
    LoadOnlineWallpaperCallback callback) {
  auto on_load = base::BindOnce(
      &OnlineWallpaperManager::OnLoadExistingOnlineWallpaperComplete,
      weak_factory_.GetWeakPtr(), wallpaper_dir, account_id, wallpaper_info,
      std::move(callback));
  wallpaper_file_manager_->LoadWallpaper(wallpaper_info.type, wallpaper_dir,
                                         wallpaper_info.location,
                                         std::move(on_load));
}

void OnlineWallpaperManager::OnLoadExistingOnlineWallpaperComplete(
    const base::FilePath& wallpaper_dir,
    const AccountId& account_id,
    const WallpaperInfo& wallpaper_info,
    LoadOnlineWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  DCHECK(callback);
  if (image.isNull()) {
    DownloadAndSaveAllVariants(wallpaper_dir, account_id, wallpaper_info,
                               std::move(callback));
  } else {
    std::move(callback).Run(image);
  }
}

void OnlineWallpaperManager::DownloadAndSaveAllVariants(
    const base::FilePath& wallpaper_dir,
    const AccountId& account_id,
    const WallpaperInfo& wallpaper_info,
    LoadOnlineWallpaperCallback callback) {
  std::vector<OnlineWallpaperVariant> variants = wallpaper_info.variants;
  if (variants.empty()) {
    // `variants` can be empty for users who have just migrated from the old
    // wallpaper picker to the new one.
    //
    // OnlineWallpaperVariant's `asset_id` and `type` are not actually used in
    // this function, so they can have dummy values here.
    variants.emplace_back(/*asset_id=*/0, GURL(wallpaper_info.location),
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
        variant.raw_url, account_id,
        base::BindOnce(
            &OnlineWallpaperManager::OnVariantDownloaded,
            weak_factory_.GetWeakPtr(), wallpaper_info.type, wallpaper_dir,
            variant.raw_url, wallpaper_info.layout,
            /*is_target_variant=*/wallpaper_info.location ==
                variant.raw_url.spec(),
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
    WallpaperType type,
    const base::FilePath& wallpaper_dir,
    const GURL& variant_url,
    WallpaperLayout layout,
    bool is_target_variant,
    VariantsDownloadResult* downloads_result,
    base::RepeatingClosure on_done,
    const gfx::ImageSkia& image) {
  DCHECK(downloads_result);
  if (image.isNull()) {
    LOG(WARNING) << "Image download failed";
    downloads_result->any_downloads_failed = true;
    std::move(on_done).Run();
    return;
  }

  if (is_target_variant) {
    downloads_result->target_variant = image;
  }

  // Using wallpaper_file_manager_->SaveWallpaperToDisk() to post a task of
  // saving the image to disk via `blocking_task_runner_` to ensure the
  // operation order is maintained. It is important this task is executed before
  // loading the preview image to make sure the files have been saved on disk.
  wallpaper_file_manager_->SaveWallpaperToDisk(
      type, wallpaper_dir, variant_url.ExtractFileName(), layout, image);

  std::move(on_done).Run();
}

}  // namespace ash
