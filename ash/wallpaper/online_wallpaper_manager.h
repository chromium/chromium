// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_ONLINE_WALLPAPER_MANAGER_H_
#define ASH_WALLPAPER_ONLINE_WALLPAPER_MANAGER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resolution.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace ash {

class WallpaperImageDownloader;

// Returns the path of the online wallpaper corresponding to `url` and
// `resolution` with the base path `wallpaper_dir`.
//
// This method is thread safe.
base::FilePath GetOnlineWallpaperPath(const base::FilePath& wallpaper_dir,
                                      const GURL& url,
                                      WallpaperResolution resolution);

// Handles loading and saving online wallpaper images for WallpaperController.
class ASH_EXPORT OnlineWallpaperManager {
 public:
  explicit OnlineWallpaperManager(
      WallpaperImageDownloader* wallpaper_image_downloader);

  OnlineWallpaperManager(const OnlineWallpaperManager&) = delete;
  OnlineWallpaperManager& operator=(const OnlineWallpaperManager&) = delete;

  ~OnlineWallpaperManager();

  // Loads a previously saved online wallpaper from `wallpaper_dir` and returns
  // it as a gfx::ImageSkia to the caller. The caller specifies which online
  // wallpaper asset to load through `url` (the url is used as a persistent file
  // identifier). The `callback` is run when the image has been loaded. A null
  // gfx::ImageSkia instance may be returned if loading the wallpaper failed;
  // this usually means the requested online wallpaper does not exist on disc.
  using LoadOnlineWallpaperCallback =
      base::OnceCallback<void(const gfx::ImageSkia&)>;
  void LoadOnlineWallpaper(const base::FilePath& wallpaper_dir,
                           const GURL& url,
                           LoadOnlineWallpaperCallback callback);

  // Loads a previously saved image from `wallpaper_dir` and returns it as a
  // scoped_refptr<base::RefCountedMemory> to be served as the preview of an
  // online wallpaper caller. The caller specifies which the asset to load
  // through `preview_url` (the url is used as a persistent file identifier).
  // The `callback` is run when the image has been loaded. A nullptr may be
  // returned if loading the image failed; this usually means the preview image
  // does not exist on disc.
  using LoadPreviewImageCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;
  void LoadOnlineWallpaperPreview(const base::FilePath& wallpaper_dir,
                                  const GURL& preview_url,
                                  LoadPreviewImageCallback callback);

  // Attempts to load the wallpaper at `params.url` by calling
  // LoadOnlineWallpaper() first. Instead of failing if loading the wallpaper is
  // unsuccessful, it tries to download the wallpaper along with other
  // wallpapers in `params.variants` over the network. Assuming all the
  // wallpapers are downloaded and saved to disk successfully in
  // `params.wallpaper_dir`, the single wallpaper at `params.url` is returned to
  // the caller via the `callback`.
  void DownloadAndSaveOnlineWallpaper(const base::FilePath& wallpaper_dir,
                                      const OnlineWallpaperParams& params,
                                      LoadOnlineWallpaperCallback callback);

 private:
  void LoadFromDisk(LoadOnlineWallpaperCallback callback,
                    const base::FilePath& file_path);

  void OnLoadExistingOnlineWallpaperComplete(
      const base::FilePath& wallpaper_dir,
      const OnlineWallpaperParams& params,
      LoadOnlineWallpaperCallback callback,
      const gfx::ImageSkia& image);

  void DownloadAndSaveAllVariants(const base::FilePath& wallpaper_dir,
                                  const OnlineWallpaperParams& params,
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

  void OnVariantDownloaded(const base::FilePath& wallpaper_dir,
                           const GURL& variant_url,
                           WallpaperLayout layout,
                           bool is_target_variant,
                           VariantsDownloadResult* downloads_result,
                           base::RepeatingClosure on_done,
                           const gfx::ImageSkia& image);

  raw_ptr<WallpaperImageDownloader> wallpaper_image_downloader_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OnlineWallpaperManager> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_ONLINE_WALLPAPER_MANAGER_H_
