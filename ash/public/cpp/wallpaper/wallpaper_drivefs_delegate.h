// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_DRIVEFS_DELEGATE_H_
#define ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_DRIVEFS_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/image_downloader.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"

namespace ash {

// Utility class to interact with DriveFS to sync user custom wallpapers.
class ASH_PUBLIC_EXPORT WallpaperDriveFsDelegate {
 public:
  virtual ~WallpaperDriveFsDelegate() = default;

  // Gets the path that synced custom wallpaper is saved to in DriveFS. Returns
  // empty path if DriveFS is inaccessible. If DriveFS is available and mounted
  // and `DriveIntegrationService` is available for `account_id`, should always
  // return a valid path.
  virtual base::FilePath GetWallpaperPath(const AccountId& account_id) = 0;

  // Copies `source` to DriveFS. This function does not check that `source` is a
  // valid jpg file, so the caller must do so. Calls `callback` when the
  // operation finishes.
  virtual void SaveWallpaper(
      const AccountId& account_id,
      const base::FilePath& source,
      base::OnceCallback<void(bool success)> callback) = 0;

  using GetWallpaperModificationTimeCallback =
      base::OnceCallback<void(base::Time modification_time)>;
  // Gets the `modification_time` of the wallpaper file saved in DriveFS. If
  // unable to retrieve it because the file does not exist or DriveFS is not
  // mounted, replies with a default constructed `base::Time()`.
  virtual void GetWallpaperModificationTime(
      const AccountId& account_id,
      GetWallpaperModificationTimeCallback callback) = 0;

  using WaitForWallpaperChangeCallback = base::OnceCallback<void(bool success)>;
  // Waits for the DriveFS wallpaper file to change. Does not distinguish
  // between types of changes, so the caller must be aware that the file could
  // have been added, deleted, or modified. Runs `callback` with success=false
  // if unable to observe for DriveFS changes for any reason, like DriveFS being
  // or becoming unavailable for `account_id`.
  virtual void WaitForWallpaperChange(
      const AccountId& account_id,
      WaitForWallpaperChangeCallback callback) = 0;

  // Downloads and decodes DriveFS wallpaper file. Replies with default
  // constructed `gfx::ImageSkia` in case of failure, such as the file not
  // existing or DriveFS error.
  virtual void DownloadAndDecodeWallpaper(
      const AccountId& account_id,
      ImageDownloader::DownloadCallback callback) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_DRIVEFS_DELEGATE_H_
