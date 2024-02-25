// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/test_wallpaper_controller_client.h"

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/time/time.h"

namespace ash {

// static
const std::string TestWallpaperControllerClient::kDummyCollectionId =
    "testCollectionId";

TestWallpaperControllerClient::TestWallpaperControllerClient() {
  std::vector<backdrop::Image>& images = variations_[kDummyCollectionId];
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
}

TestWallpaperControllerClient::~TestWallpaperControllerClient() = default;

void TestWallpaperControllerClient::AddCollection(
    const std::string& collection_id,
    const std::vector<backdrop::Image>& images) {
  variations_[collection_id] = images;
}

void TestWallpaperControllerClient::ResetCounts() {
  open_count_ = 0;
  fetch_daily_refresh_wallpaper_param_ = std::string();
  fetch_daily_refresh_info_fails_ = false;
  fake_files_ids_.clear();
  wallpaper_sync_enabled_ = true;
}

// WallpaperControllerClient:
void TestWallpaperControllerClient::OpenWallpaperPicker() {
  open_count_++;
}

void TestWallpaperControllerClient::FetchDailyRefreshWallpaper(
    const std::string& collection_id,
    DailyWallpaperUrlFetchedCallback callback) {
  auto iter = variations_.find(collection_id);
  fetch_daily_refresh_wallpaper_param_ = collection_id;
  if (fetch_daily_refresh_info_fails_ || iter == variations_.end()) {
    std::move(callback).Run(/*success=*/false, std::move(backdrop::Image()));
    return;
  }

  image_index_ = ++image_index_ % iter->second.size();
  backdrop::Image image(iter->second.at(image_index_));
  std::move(callback).Run(/*success=*/true, std::move(image));
}

void TestWallpaperControllerClient::FetchImagesForCollection(
    const std::string& collection_id,
    FetchImagesForCollectionCallback callback) {
  fetch_images_for_collection_count_++;
  auto iter = variations_.find(collection_id);
  if (fetch_images_for_collection_fails_ || iter == variations_.end()) {
    std::move(callback).Run(/*success=*/false, std::vector<backdrop::Image>());
    return;
  }

  std::vector<backdrop::Image> images = iter->second;
  std::move(callback).Run(/*success=*/true, std::move(images));
}

void TestWallpaperControllerClient::FetchGooglePhotosPhoto(
    const AccountId& account_id,
    const std::string& id,
    FetchGooglePhotosPhotoCallback callback) {
  fetch_google_photos_photo_id_ = id;
  auto iter = wallpaper_google_photos_integration_enabled_.find(account_id);
  if (iter != wallpaper_google_photos_integration_enabled_.end() &&
      !iter->second) {
    std::move(callback).Run(/*photo=*/nullptr, /*success=*/true);
    return;
  }
  base::Time time;
  static constexpr base::Time::Exploded kTime = {.year = 2011,
                                                 .month = 6,
                                                 .day_of_week = 3,
                                                 .day_of_month = 15,
                                                 .hour = 12};
  CHECK(base::Time::FromUTCExploded(kTime, &time));
  if (fetch_google_photos_photo_fails_ || google_photo_has_been_deleted_) {
    std::move(callback).Run(nullptr,
                            /*success=*/google_photo_has_been_deleted_);
  } else {
    std::move(callback).Run(
        personalization_app::mojom::GooglePhotosPhoto::New(
            id, "dedup_key", "test_name", base::TimeFormatFriendlyDate(time),
            GURL("https://google.com/picture.png"), "home"),
        /*success=*/true);
  }
}

void TestWallpaperControllerClient::FetchDailyGooglePhotosPhoto(
    const AccountId& account_id,
    const std::string& album_id,
    FetchGooglePhotosPhotoCallback callback) {
  std::string photo_id = album_id;
  std::reverse(photo_id.begin(), photo_id.end());
  FetchGooglePhotosPhoto(account_id, photo_id, std::move(callback));
}

void TestWallpaperControllerClient::FetchGooglePhotosAccessToken(
    const AccountId& account_id,
    FetchGooglePhotosAccessTokenCallback callback) {
  std::move(callback).Run(std::nullopt);
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
