// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_TEST_WALLPAPER_DRIVEFS_DELEGATE_H_
#define ASH_WALLPAPER_TEST_WALLPAPER_DRIVEFS_DELEGATE_H_

#include "ash/public/cpp/wallpaper/wallpaper_drivefs_delegate.h"

#include "ash/public/cpp/image_downloader.h"
#include "base/files/file_path.h"
#include "components/account_id/account_id.h"

namespace ash {

class TestWallpaperDriveFsDelegate : public WallpaperDriveFsDelegate {
 public:
  TestWallpaperDriveFsDelegate();

  TestWallpaperDriveFsDelegate(const TestWallpaperDriveFsDelegate&) = delete;
  TestWallpaperDriveFsDelegate& operator=(const TestWallpaperDriveFsDelegate&) =
      delete;

  ~TestWallpaperDriveFsDelegate() override;

  void Reset();

  AccountId get_save_wallpaper_account_id() const {
    return save_wallpaper_account_id_;
  }

  AccountId get_wallpaper_path_account_id() const {
    return get_wallpaper_path_account_id_;
  }

  // WallpaperDriveFsDelegate:
  base::FilePath GetWallpaperPath(const AccountId& account_id) override;
  void SaveWallpaper(const AccountId& account_id,
                     const base::FilePath& source,
                     base::OnceCallback<void(bool success)> callback) override;
  // Returns the current time as the modification time as the callback expects
  // it to be greater than the current wallpaper's set time to proceed.
  void GetWallpaperModificationTime(
      const AccountId& account_id,
      GetWallpaperModificationTimeCallback callback) override;
  // Returns true to signal the wallpaper file has changed successfully.
  void WaitForWallpaperChange(const AccountId& account_id,
                              WaitForWallpaperChangeCallback callback) override;
  void DownloadAndDecodeWallpaper(
      const AccountId& account_id,
      ImageDownloader::DownloadCallback callback) override;

 private:
  AccountId save_wallpaper_account_id_;
  AccountId get_wallpaper_path_account_id_;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_TEST_WALLPAPER_DRIVEFS_DELEGATE_H_
