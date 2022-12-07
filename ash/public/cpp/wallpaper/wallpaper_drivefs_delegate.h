// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_DRIVEFS_DELEGATE_H_
#define ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_DRIVEFS_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"

namespace ash {

// Utility class to interact with DriveFS to sync user custom wallpapers.
class ASH_PUBLIC_EXPORT WallpaperDriveFsDelegate {
 public:
  virtual ~WallpaperDriveFsDelegate() = default;

  // Gets the `modification_time` of the wallpaper file saved in DriveFS. If
  // unable to retrieve it because the file does not exist or DriveFS is not
  // mounted, replies with a default constructed `base::Time()`.
  virtual void GetWallpaperModificationTime(
      const AccountId& account_id,
      base::OnceCallback<void(base::Time modification_time)> callback) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_DRIVEFS_DELEGATE_H_
