// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_TEST_WALLPAPER_CONTROLLER_CLIENT_H_
#define ASH_WALLPAPER_TEST_WALLPAPER_CONTROLLER_CLIENT_H_

#include <stddef.h>

#include <unordered_map>

#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "components/account_id/account_id.h"

namespace ash {

// A test wallpaper controller client class.
class TestWallpaperControllerClient : public WallpaperControllerClient {
 public:
  // A preconfigured collection of wallpaper variants that will return some
  // usable values.
  static const std::string kDummyCollectionId;

  TestWallpaperControllerClient();
  TestWallpaperControllerClient(const TestWallpaperControllerClient&) = delete;
  TestWallpaperControllerClient& operator=(
      const TestWallpaperControllerClient&) = delete;
  virtual ~TestWallpaperControllerClient();

  // Add a set of |images| for |collection_id| that will be returned for
  // FetchDailyRefreshWallpaper and FetchImagesForCollection.
  void AddCollection(const std::string& collection_id,
                     const std::vector<backdrop::Image>& images);

  size_t open_count() const { return open_count_; }
  size_t fetch_images_for_collection_count() const {
    return fetch_images_for_collection_count_;
  }
  std::string fetch_google_photos_photo_id() const {
    return fetch_google_photos_photo_id_;
  }
  std::string get_fetch_daily_refresh_wallpaper_param() const {
    return fetch_daily_refresh_wallpaper_param_;
  }

  void set_fetch_daily_refresh_info_fails(bool fails) {
    fetch_daily_refresh_info_fails_ = fails;
  }

  void set_fetch_google_photos_photo_fails(bool fails) {
    fetch_google_photos_photo_fails_ = fails;
  }

  void set_google_photo_has_been_deleted(bool deleted) {
    google_photo_has_been_deleted_ = deleted;
  }

  void set_fake_files_id_for_account_id(const AccountId& account_id,
                                        std::string fake_files_id) {
    fake_files_ids_[account_id] = fake_files_id;
  }

  void set_wallpaper_sync_enabled(bool sync_enabled) {
    wallpaper_sync_enabled_ = sync_enabled;
  }

  void set_wallpaper_google_photos_integration_enabled_for_account_id(
      const AccountId& account_id,
      bool value) {
    wallpaper_google_photos_integration_enabled_[account_id] = value;
  }

  void ResetCounts();

  // WallpaperControllerClient:
  void OpenWallpaperPicker() override;
  void FetchDailyRefreshWallpaper(
      const std::string& collection_id,
      DailyWallpaperUrlFetchedCallback callback) override;
  void FetchImagesForCollection(
      const std::string& collection_id,
      FetchImagesForCollectionCallback callback) override;
  void FetchGooglePhotosPhoto(const AccountId& account_id,
                              const std::string& id,
                              FetchGooglePhotosPhotoCallback callback) override;
  void FetchDailyGooglePhotosPhoto(
      const AccountId& account_id,
      const std::string& album_id,
      FetchGooglePhotosPhotoCallback callback) override;
  void FetchGooglePhotosAccessToken(
      const AccountId& account_id,
      FetchGooglePhotosAccessTokenCallback callback) override;
  void GetFilesId(const AccountId& account_id,
                  base::OnceCallback<void(const std::string&)>
                      files_id_callback) const override;
  bool IsWallpaperSyncEnabled(const AccountId& account_id) const override;

 private:
  size_t open_count_ = 0;
  size_t fetch_images_for_collection_count_ = 0;
  std::string fetch_google_photos_photo_id_;
  std::string fetch_daily_refresh_wallpaper_param_;
  bool fetch_daily_refresh_info_fails_ = false;
  std::unordered_map<AccountId, std::string> fake_files_ids_;
  bool wallpaper_sync_enabled_ = true;
  bool fetch_images_for_collection_fails_ = false;
  bool fetch_google_photos_photo_fails_ = false;
  bool google_photo_has_been_deleted_ = false;
  std::map<AccountId, bool> wallpaper_google_photos_integration_enabled_;

  int image_index_ = 0;
  base::flat_map<std::string, std::vector<backdrop::Image>> variations_;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_TEST_WALLPAPER_CONTROLLER_CLIENT_H_
