// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_blur_manager.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"

namespace ash {

WallpaperBlurManager::WallpaperBlurManager() = default;

WallpaperBlurManager::~WallpaperBlurManager() = default;

bool WallpaperBlurManager::IsBlurAllowedForLockState(
    const WallpaperType wallpaper_type) const {
  switch (wallpaper_type) {
    // kDevice is never blurred: https://crbug.com/775591.
    case WallpaperType::kDevice:
      return false;
    case WallpaperType::kOneShot:
      return allow_blur_for_testing_;
    case WallpaperType::kDaily:
    case WallpaperType::kCustomized:
    case WallpaperType::kDefault:
    case WallpaperType::kOnline:
    case WallpaperType::kPolicy:
    case WallpaperType::kThirdParty:
    case WallpaperType::kDailyGooglePhotos:
    case WallpaperType::kOnceGooglePhotos:
    case WallpaperType::kOobe:
    // May receive kCount if wallpaper not loaded yet.
    case WallpaperType::kCount:
      return true;
  }
}

}  // namespace ash
