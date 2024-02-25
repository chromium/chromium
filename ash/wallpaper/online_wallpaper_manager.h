// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_ONLINE_WALLPAPER_MANAGER_H_
#define ASH_WALLPAPER_ONLINE_WALLPAPER_MANAGER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/wallpaper/wallpaper_file_manager.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resolution.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

class WallpaperImageDownloader;

// Handles loading and saving online wallpaper images for WallpaperController.
class ASH_EXPORT OnlineWallpaperManager {
 public:
  explicit OnlineWallpaperManager(
      WallpaperImageDownloader* wallpaper_image_downloader,
      WallpaperFileManager* wallpaper_file_manager);

  OnlineWallpaperManager(const OnlineWallpaperManager&) = delete;
  OnlineWallpaperManager& operator=(const OnlineWallpaperManager&) = delete;

  ~OnlineWallpaperManager();

  using LoadOnlineWallpaperCallback =
      base::OnceCallback<void(const gfx::ImageSkia&)>;
  // Attempts to load the wallpaper from the disk first. Instead of failing if
  // loading the wallpaper is unsuccessful, it tries to download the wallpaper
  // along with other wallpapers in `wallpaper_info.variants` over the network.
  void GetOnlineWallpaper(const base::FilePath& wallpaper_dir,
                          const AccountId& account_id,
                          const WallpaperInfo& wallpaper_info,
                          LoadOnlineWallpaperCallback callback);

 private:
  void OnLoadExistingOnlineWallpaperComplete(
      const base::FilePath& wallpaper_dir,
      const AccountId& account_id,
      const WallpaperInfo& wallpaper_info,
      LoadOnlineWallpaperCallback callback,
      const gfx::ImageSkia& image);

  void DownloadAndSaveAllVariants(const base::FilePath& wallpaper_dir,
                                  const AccountId& account_id,
                                  const WallpaperInfo& wallpaper_info,
                                  LoadOnlineWallpaperCallback callback);

  // Just bundles together the ultimate output needed in
  // OnAllVariantsDownloaded(). Gets filled in incrementally as each individual
  // variant is downloaded.
  struct VariantsDownloadResult {
    // Filled with the variant that was requested by the caller and is needed
    // immediately.
    gfx::ImageSkia target_variant;
    bool any_downloads_failed = false;
  };
  void OnAllVariantsDownloaded(
      std::unique_ptr<VariantsDownloadResult> downloads_result,
      LoadOnlineWallpaperCallback callback);

  void OnVariantDownloaded(WallpaperType type,
                           const base::FilePath& wallpaper_dir,
                           const GURL& variant_url,
                           WallpaperLayout layout,
                           bool is_target_variant,
                           VariantsDownloadResult* downloads_result,
                           base::RepeatingClosure on_done,
                           const gfx::ImageSkia& image);

  raw_ptr<WallpaperImageDownloader> wallpaper_image_downloader_;

  raw_ptr<WallpaperFileManager> wallpaper_file_manager_;

  base::WeakPtrFactory<OnlineWallpaperManager> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_ONLINE_WALLPAPER_MANAGER_H_
