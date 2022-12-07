// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_WALLPAPER_DRIVEFS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_WALLPAPER_WALLPAPER_DRIVEFS_DELEGATE_IMPL_H_

#include "ash/public/cpp/wallpaper/wallpaper_drivefs_delegate.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"

namespace ash {

class WallpaperDriveFsDelegateImpl : public WallpaperDriveFsDelegate {
 public:
  WallpaperDriveFsDelegateImpl();

  WallpaperDriveFsDelegateImpl(const WallpaperDriveFsDelegateImpl&) = delete;
  WallpaperDriveFsDelegateImpl& operator=(const WallpaperDriveFsDelegateImpl&) =
      delete;

  ~WallpaperDriveFsDelegateImpl() override;

  // WallpaperDriveFsDelegate:
  void GetWallpaperModificationTime(
      const AccountId& account_id,
      base::OnceCallback<void(base::Time modification_time)> callback) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WALLPAPER_WALLPAPER_DRIVEFS_DELEGATE_IMPL_H_
