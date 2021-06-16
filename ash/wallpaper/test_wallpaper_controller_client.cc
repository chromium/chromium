// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/test_wallpaper_controller_client.h"

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
  save_wallpaper_to_drive_fs_account_id.clear();
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

void TestWallpaperControllerClient::MigrateCollectionIdFromChromeApp() {
  migrate_collection_id_from_chrome_app_count_++;
}

void TestWallpaperControllerClient::FetchDailyRefreshWallpaper(
    const std::string& collection_id,
    DailyWallpaperUrlFetchedCallback callback) {
  fetch_daily_refresh_wallpaper_param_ = collection_id;
  std::move(callback).Run(fetch_daily_refresh_info_fails_ ? std::string()
                                                          : "fun_image_url");
}

void TestWallpaperControllerClient::SaveWallpaperToDriveFs(
    const AccountId& account_id,
    const base::FilePath& origin) {
  save_wallpaper_to_drive_fs_account_id = account_id;
}

}  // namespace ash
