// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/google_photos_wallpaper_manager.h"

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_image_downloader.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image.h"

namespace ash {

GooglePhotosWallpaperManager::GooglePhotosWallpaperManager(
    WallpaperImageDownloader* wallpaper_image_downloader,
    WallpaperFileManager* wallpaper_file_manager)
    : wallpaper_image_downloader_(wallpaper_image_downloader),
      wallpaper_file_manager_(wallpaper_file_manager) {}

GooglePhotosWallpaperManager::~GooglePhotosWallpaperManager() = default;

void GooglePhotosWallpaperManager::SetClient(
    WallpaperControllerClient* client) {
  wallpaper_controller_client_ = client;
}

void GooglePhotosWallpaperManager::GetGooglePhotosWallpaper(
    const base::FilePath& wallpaper_dir,
    const GooglePhotosWallpaperParams& params,
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    LoadGooglePhotosWallpaperCallback callback) {
  const std::string location = photo->id;
  auto on_load = base::BindOnce(&GooglePhotosWallpaperManager::
                                    OnLoadExistingGooglePhotosWallpaperComplete,
                                weak_factory_.GetWeakPtr(), wallpaper_dir,
                                params, std::move(photo), std::move(callback));
  wallpaper_file_manager_->LoadWallpaper(
      params.daily_refresh_enabled ? WallpaperType::kDailyGooglePhotos
                                   : WallpaperType::kOnceGooglePhotos,
      wallpaper_dir, location, std::move(on_load));
}

void GooglePhotosWallpaperManager::DownloadGooglePhotosWallpaper(
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    const AccountId& account_id,
    LoadGooglePhotosWallpaperCallback callback) {
  wallpaper_controller_client_->FetchGooglePhotosAccessToken(
      account_id, base::BindOnce(&GooglePhotosWallpaperManager::
                                     OnGooglePhotosAuthenticationTokenFetched,
                                 weak_factory_.GetWeakPtr(), std::move(photo),
                                 account_id, std::move(callback)));
}

void GooglePhotosWallpaperManager::OnGooglePhotosAuthenticationTokenFetched(
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    const AccountId& account_id,
    LoadGooglePhotosWallpaperCallback callback,
    const std::optional<std::string>& access_token) {
  wallpaper_image_downloader_->DownloadGooglePhotosImage(
      photo->url, account_id, access_token, std::move(callback));
}

void GooglePhotosWallpaperManager::OnGooglePhotosWallpaperDownloaded(
    const WallpaperType type,
    const base::FilePath& wallpaper_dir,
    const std::string& photo_id,
    const WallpaperLayout layout,
    LoadGooglePhotosWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  wallpaper_file_manager_->SaveWallpaperToDisk(type, wallpaper_dir, photo_id,
                                               layout, image);
  std::move(callback).Run(image);
}

void GooglePhotosWallpaperManager::OnLoadExistingGooglePhotosWallpaperComplete(
    const base::FilePath& wallpaper_dir,
    const GooglePhotosWallpaperParams& params,
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    LoadGooglePhotosWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  DCHECK(callback);
  if (image.isNull()) {
    auto on_download = base::BindOnce(
        &GooglePhotosWallpaperManager::OnGooglePhotosWallpaperDownloaded,
        weak_factory_.GetWeakPtr(),
        params.daily_refresh_enabled ? WallpaperType::kDailyGooglePhotos
                                     : WallpaperType::kOnceGooglePhotos,
        wallpaper_dir, photo->id, params.layout, std::move(callback));
    DownloadGooglePhotosWallpaper(std::move(photo), params.account_id,
                                  std::move(on_download));
  } else {
    std::move(callback).Run(image);
  }
}

}  // namespace ash
