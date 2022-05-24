// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_CONTROLLER_CLIENT_H_
#define ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_CONTROLLER_CLIENT_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"

class AccountId;

namespace ash {

namespace personalization_app::mojom {
class GooglePhotosPhoto;
}

// Used by ash to control a Chrome client of the WallpaperController.
class ASH_PUBLIC_EXPORT WallpaperControllerClient {
 public:
  // Opens the wallpaper picker window.
  virtual void OpenWallpaperPicker() = 0;

  // Closes the app side of the wallpaper preview (top header bar) if it is
  // currently open.
  virtual void MaybeClosePreviewWallpaper() = 0;

  // Sets the default wallpaper and removes the file for the previous wallpaper.
  virtual void SetDefaultWallpaper(
      const AccountId& account_id,
      bool show_wallpaper,
      base::OnceCallback<void(bool success)> callback) = 0;

  // Retrieves the current collection id from the Wallpaper Picker Chrome App
  // for migration and returns it via |result_callback|. The string in
  // |result_callback| will be empty if the fetch failed.
  virtual void MigrateCollectionIdFromChromeApp(
      const AccountId& account_id,
      base::OnceCallback<void(const std::string&)> result_callback) = 0;

  // Downloads and sets a new random wallpaper from the collection of the
  // specified collection_id.
  using DailyWallpaperUrlFetchedCallback =
      base::OnceCallback<void(bool success, const backdrop::Image& image)>;
  virtual void FetchDailyRefreshWallpaper(
      const std::string& collection_id,
      DailyWallpaperUrlFetchedCallback callback) = 0;

  virtual void SaveWallpaperToDriveFs(
      const AccountId& account_id,
      const base::FilePath& origin,
      base::OnceCallback<void(bool)> wallpaper_saved_callback) = 0;

  virtual base::FilePath GetWallpaperPathFromDriveFs(
      const AccountId& account_id) = 0;

  virtual void GetFilesId(
      const AccountId& account_id,
      base::OnceCallback<void(const std::string&)> files_id_callback) const = 0;

  virtual bool IsWallpaperSyncEnabled(const AccountId& account_id) const = 0;

  using FetchImagesForCollectionCallback =
      base::OnceCallback<void(bool success,
                              const std::vector<backdrop::Image>& images)>;
  virtual void FetchImagesForCollection(
      const std::string& collection_id,
      FetchImagesForCollectionCallback callback) = 0;

  using FetchGooglePhotosPhotoCallback = base::OnceCallback<void(
      mojo::StructPtr<ash::personalization_app::mojom::GooglePhotosPhoto>,
      bool success)>;
  virtual void FetchGooglePhotosPhoto(
      const AccountId& account_id,
      const std::string& id,
      FetchGooglePhotosPhotoCallback callback) = 0;
  virtual void FetchDailyGooglePhotosPhoto(
      const AccountId& account_id,
      const std::string& album_id,
      FetchGooglePhotosPhotoCallback callback) = 0;

  using FetchGooglePhotosAccessTokenCallback =
      base::OnceCallback<void(const absl::optional<std::string>& token)>;
  virtual void FetchGooglePhotosAccessToken(
      const AccountId& account_id,
      FetchGooglePhotosAccessTokenCallback callback) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_CONTROLLER_CLIENT_H_
