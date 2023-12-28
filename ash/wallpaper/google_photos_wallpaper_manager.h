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
#include "ash/wallpaper/wallpaper_file_manager.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

class WallpaperImageDownloader;

// Handles loading and downloading google photos wallpaper images for
// WallpaperController.
class ASH_EXPORT GooglePhotosWallpaperManager {
 public:
  explicit GooglePhotosWallpaperManager(
      WallpaperImageDownloader* wallpaper_image_downloader,
      WallpaperFileManager* wallpaper_file_manager);

  GooglePhotosWallpaperManager(const GooglePhotosWallpaperManager&) = delete;
  GooglePhotosWallpaperManager& operator=(const GooglePhotosWallpaperManager&) =
      delete;

  ~GooglePhotosWallpaperManager();

  void SetClient(WallpaperControllerClient* client);

  using LoadGooglePhotosWallpaperCallback =
      base::OnceCallback<void(const gfx::ImageSkia&)>;

  // Attempts to load the Google Photos wallpaper from disk. If loading the
  // wallpaper is unsuccessful, it tries to download the wallpaper.
  void GetGooglePhotosWallpaper(
      const base::FilePath& wallpaper_dir,
      const GooglePhotosWallpaperParams& params,
      ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
      LoadGooglePhotosWallpaperCallback callback);

 private:
  void DownloadGooglePhotosWallpaper(
      ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
      const AccountId& account_id,
      LoadGooglePhotosWallpaperCallback callback);

  void OnGooglePhotosAuthenticationTokenFetched(
      ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
      const AccountId& account_id,
      LoadGooglePhotosWallpaperCallback callback,
      const std::optional<std::string>& access_token);

  void OnGooglePhotosWallpaperDownloaded(
      const WallpaperType type,
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

  raw_ptr<WallpaperControllerClient> wallpaper_controller_client_;

  raw_ptr<WallpaperFileManager> wallpaper_file_manager_;

  base::WeakPtrFactory<GooglePhotosWallpaperManager> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_GOOGLE_PHOTOS_WALLPAPER_MANAGER_H_
