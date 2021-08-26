// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/test_wallpaper_controller_client.h"

#include "base/logging.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
    std::move(callback).Run(absl::nullopt, std::string());
  } else {
    std::move(callback).Run(1, "fun_image_url");
  }
}

bool TestWallpaperControllerClient::SaveWallpaperToDriveFs(
    const AccountId& account_id,
    const base::FilePath& origin) {
  save_wallpaper_to_drive_fs_account_id_ = account_id;
  return true;
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
