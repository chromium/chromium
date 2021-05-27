// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/test_wallpaper_controller_client.h"

namespace ash {

void TestWallpaperControllerClient::ResetCounts() {
  open_count_ = 0;
  close_preview_count_ = 0;
  set_default_wallpaper_count_ = 0;
  migrate_collection_id_from_chrome_app_count_ = 0;
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

}  // namespace ash
