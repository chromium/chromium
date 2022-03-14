// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/test_wallpaper_controller_client.h"

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/time/time.h"

namespace ash {

TestWallpaperControllerClient::TestWallpaperControllerClient() = default;
TestWallpaperControllerClient::~TestWallpaperControllerClient() = default;

void TestWallpaperControllerClient::ResetCounts() {
  open_count_ = 0;
  close_preview_count_ = 0;
  set_default_wallpaper_count_ = 0;
  migrate_collection_id_from_chrome_app_count_ = 0;
  fetch_daily_refresh_wallpaper_param_ = std::string();
  fetch_daily_refresh_info_fails_ = false;
  get_wallpaper_path_from_drive_fs_account_id_.clear();
  save_wallpaper_to_drive_fs_account_id_.clear();
  fake_files_ids_.clear();
  wallpaper_sync_enabled_ = true;
}

// WallpaperControllerClient:
void TestWallpaperControllerClient::OpenWallpaperPicker() {
  open_count_++;
}

void TestWallpaperControllerClient::MaybeClosePreviewWallpaper() {
  close_preview_count_++;
}

void TestWallpaperControllerClient::SetDefaultWallpaper(
    const AccountId& account_id,
    bool show_wallpaper) {
  set_default_wallpaper_count_++;
}

void TestWallpaperControllerClient::MigrateCollectionIdFromChromeApp(
    const AccountId& account_id) {
  migrate_collection_id_from_chrome_app_count_++;
}

void TestWallpaperControllerClient::FetchDailyRefreshWallpaper(
    const std::string& collection_id,
    DailyWallpaperUrlFetchedCallback callback) {
  fetch_daily_refresh_wallpaper_param_ = collection_id;
  if (fetch_daily_refresh_info_fails_) {
    std::move(callback).Run(/*success=*/false, std::move(backdrop::Image()));
  } else {
    backdrop::Image image;
    image.set_asset_id(1);
    image.set_image_url("http://example.com");
    image.add_attribution()->set_text("test");
    image.set_unit_id(1);
    image.set_image_type(backdrop::Image_ImageType_IMAGE_TYPE_LIGHT_MODE);
    std::move(callback).Run(/*success=*/true, std::move(image));
  }
}

void TestWallpaperControllerClient::FetchImagesForCollection(
    const std::string& collection_id,
    FetchImagesForCollectionCallback callback) {
  if (fetch_images_for_collection_fails_) {
    std::move(callback).Run(/*success=*/false, std::vector<backdrop::Image>());
  } else {
    std::vector<backdrop::Image> images;

    backdrop::Image image1;
    image1.set_asset_id(1);
    image1.set_image_url("https://best_wallpaper/1");
    image1.add_attribution()->set_text("test");
    image1.set_unit_id(1);
    image1.set_image_type(backdrop::Image::IMAGE_TYPE_DARK_MODE);
    images.push_back(image1);

    backdrop::Image image2;
    image2.set_asset_id(2);
    image2.set_image_url("https://best_wallpaper/2");
    image2.add_attribution()->set_text("test");
    image2.set_unit_id(1);
    image2.set_image_type(backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
    images.push_back(image2);

    std::move(callback).Run(/*success=*/true, std::move(images));
  }
}

void TestWallpaperControllerClient::FetchGooglePhotosPhoto(
    const AccountId& account_id,
    const std::string& id,
    FetchGooglePhotosPhotoCallback callback) {
  base::Time time;
  base::Time::Exploded exploded_time{2011, 6, 3, 15, 12, 0, 0, 0};
  DCHECK(base::Time::FromUTCExploded(exploded_time, &time));
  if (fetch_google_photos_photo_fails_) {
    std::move(callback).Run(nullptr);
  } else {
    std::move(callback).Run(personalization_app::mojom::GooglePhotosPhoto::New(
        id, "test_name", base::TimeFormatFriendlyDate(time),
        GURL("https://google.com/picture.png")));
  }
}

void TestWallpaperControllerClient::SaveWallpaperToDriveFs(
    const AccountId& account_id,
    const base::FilePath& origin,
    base::OnceCallback<void(bool)> wallpaper_saved_callback) {
  save_wallpaper_to_drive_fs_account_id_ = account_id;
  std::move(wallpaper_saved_callback).Run(true);
}

base::FilePath TestWallpaperControllerClient::GetWallpaperPathFromDriveFs(
    const AccountId& account_id) {
  get_wallpaper_path_from_drive_fs_account_id_ = account_id;
  return base::FilePath();
}

void TestWallpaperControllerClient::GetFilesId(
    const AccountId& account_id,
    base::OnceCallback<void(const std::string&)> files_id_callback) const {
  auto iter = fake_files_ids_.find(account_id);
  if (iter == fake_files_ids_.end()) {
    LOG(ERROR) << "No fake files id for account id: " << account_id;
    return;
  }
  std::move(files_id_callback).Run(iter->second);
}

bool TestWallpaperControllerClient::IsWallpaperSyncEnabled(
    const AccountId& account_id) const {
  return wallpaper_sync_enabled_;
}

}  // namespace ash
