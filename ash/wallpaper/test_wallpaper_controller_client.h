// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_TEST_WALLPAPER_CONTROLLER_CLIENT_H_
#define ASH_WALLPAPER_TEST_WALLPAPER_CONTROLLER_CLIENT_H_

#include <stddef.h>

#include "ash/public/cpp/wallpaper_controller_client.h"

namespace ash {

// A test wallpaper controller client class.
class TestWallpaperControllerClient : public WallpaperControllerClient {
 public:
  TestWallpaperControllerClient() = default;

  TestWallpaperControllerClient(const TestWallpaperControllerClient&) = delete;
  TestWallpaperControllerClient& operator=(
      const TestWallpaperControllerClient&) = delete;

  virtual ~TestWallpaperControllerClient() = default;

  size_t open_count() const { return open_count_; }
  size_t close_preview_count() const { return close_preview_count_; }
  size_t set_default_wallpaper_count() const {
    return set_default_wallpaper_count_;
  }
  size_t migrate_collection_id_from_chrome_app_count() const {
    return migrate_collection_id_from_chrome_app_count_;
  }

  void ResetCounts();

  // WallpaperControllerClient:
  void OpenWallpaperPicker() override;
  void MaybeClosePreviewWallpaper() override;
  void SetDefaultWallpaper(const AccountId& account_id,
                           bool show_wallpaper) override;
  void MigrateCollectionIdFromChromeApp() override;

 private:
  size_t open_count_ = 0;
  size_t close_preview_count_ = 0;
  size_t set_default_wallpaper_count_ = 0;
  size_t migrate_collection_id_from_chrome_app_count_ = 0;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_TEST_WALLPAPER_CONTROLLER_CLIENT_H_
