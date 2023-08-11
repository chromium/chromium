// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_GOOGLE_PHOTOS_WALLPAPER_MANAGER_H_
#define ASH_WALLPAPER_GOOGLE_PHOTOS_WALLPAPER_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
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

// Handles loading and downloading google photos wallpaper images for
// WallpaperController.
class ASH_EXPORT GooglePhotosWallpaperManager {
 public:
  explicit GooglePhotosWallpaperManager(
      WallpaperImageDownloader* wallpaper_image_downloader);

  GooglePhotosWallpaperManager(const GooglePhotosWallpaperManager&) = delete;
  GooglePhotosWallpaperManager& operator=(const GooglePhotosWallpaperManager&) =
      delete;

  ~GooglePhotosWallpaperManager();

  void SetClient(WallpaperControllerClient* client);

  // Loads a previously saved google photos wallpaper from disk
  // as a gfx::ImageSkia to the caller. The `callback` is run when the image has
  // been loaded. A null gfx::ImageSkia instance may be returned if loading the
  // Google Photos wallpaper failed; this usually means the requested Google
  // Photos wallpaper does not exist on disk.
  using LoadGooglePhotosWallpaperCallback =
      base::OnceCallback<void(const gfx::ImageSkia&)>;
  void LoadGooglePhotosWallpaper(const base::FilePath& file_path,
                                 LoadGooglePhotosWallpaperCallback callback);

  // Attempts to load the Google Photos wallpaper from disk by calling
  // LoadGooglePhotosWallpaper() first. If loading the wallpaper is
  // unsuccessful, it tries to download the wallpaper
  // DownloadGooglePhotosWallpaper(). Assuming the Google Photos wallpaper is
  // downloaded and saving to disk successfully, the single wallpaper image is
  // returned to the caller via the `callback`.
  void GetGooglePhotosWallpaper(
      const base::FilePath& wallpaper_dir,
      const GooglePhotosWallpaperParams& params,
      ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
      LoadGooglePhotosWallpaperCallback callback);

 private:
  void LoadFromDisk(const base::FilePath& file_path,
                    LoadGooglePhotosWallpaperCallback callback,
                    bool file_path_exists);

  void DownloadGooglePhotosWallpaper(
      ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
      const AccountId& account_id,
      LoadGooglePhotosWallpaperCallback callback);

  void OnGooglePhotosAuthenticationTokenFetched(
      ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
      const AccountId& account_id,
      LoadGooglePhotosWallpaperCallback callback,
      const absl::optional<std::string>& access_token);

  void OnGooglePhotosWallpaperDownloaded(
      const base::FilePath& wallpaper_dir,
      const std::string& photo_id,
      const WallpaperLayout layout,
      LoadGooglePhotosWallpaperCallback callback,
      const gfx::ImageSkia& image);

  void OnLoadExistingGooglePhotosWallpaperComplete(
      const base::FilePath& wallpaper_dir,
      const GooglePhotosWallpaperParams& params,
      ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
      LoadGooglePhotosWallpaperCallback callback,
      const gfx::ImageSkia& image);

  raw_ptr<WallpaperImageDownloader> wallpaper_image_downloader_;

  raw_ptr<WallpaperControllerClient, ExperimentalAsh>
      wallpaper_controller_client_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GooglePhotosWallpaperManager> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_GOOGLE_PHOTOS_WALLPAPER_MANAGER_H_
