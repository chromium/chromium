// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/wallpaper_types.h"

#include "base/logging.h"
#include "base/types/cxx23_to_underlying.h"

namespace ash {

bool IsAllowedInPrefs(WallpaperType type) {
  switch (type) {
    case WallpaperType::kOobe:
    case WallpaperType::kOneShot:
    case WallpaperType::kDevice:
    // `kThirdParty` is actually saved to `WallpaperInfo` pref as `kCustomized`.
    case WallpaperType::kThirdParty:
    case WallpaperType::kCount:
      return false;
    case WallpaperType::kDaily:
    case WallpaperType::kCustomized:
    case WallpaperType::kDefault:
    case WallpaperType::kOnline:
    case WallpaperType::kPolicy:
    case WallpaperType::kDailyGooglePhotos:
    case WallpaperType::kOnceGooglePhotos:
    case WallpaperType::kSeaPen:
      return true;
  }
  LOG(ERROR) << __func__
             << " Unknown wallpaper type: " << base::to_underlying(type);
  return false;
}

bool IsWallpaperTypeSyncable(WallpaperType type) {
  switch (type) {
    case WallpaperType::kDaily:
    case WallpaperType::kCustomized:
    case WallpaperType::kOnline:
    case WallpaperType::kOnceGooglePhotos:
    case WallpaperType::kDailyGooglePhotos:
      return true;
    case WallpaperType::kDefault:
    case WallpaperType::kPolicy:
    case WallpaperType::kThirdParty:
    case WallpaperType::kDevice:
    case WallpaperType::kOneShot:
    case WallpaperType::kOobe:
    case WallpaperType::kSeaPen:
    case WallpaperType::kCount:
      return false;
  }
  LOG(WARNING) << __func__
               << " Unknown wallpaper type: " << base::to_underlying(type);
  return false;
}

bool IsOnlineWallpaper(WallpaperType type) {
  return type == WallpaperType::kDaily || type == WallpaperType::kOnline;
}

base::Version GetSupportedVersion(WallpaperType type) {
  switch (type) {
    case WallpaperType::kOnline:
    case WallpaperType::kDaily:
    case WallpaperType::kOnceGooglePhotos:
    case WallpaperType::kDailyGooglePhotos:
    case WallpaperType::kCustomized:
    case WallpaperType::kDefault:
    case WallpaperType::kPolicy:
    case WallpaperType::kThirdParty:
    case WallpaperType::kDevice:
    case WallpaperType::kOneShot:
    case WallpaperType::kOobe:
    case WallpaperType::kSeaPen:
      return base::Version("1.0");
    case WallpaperType::kCount:
      LOG(WARNING) << __func__
                   << " Unknown wallpaper type: " << base::to_underlying(type);
      return base::Version();
  }
  LOG(WARNING) << __func__
               << " Unknown wallpaper type: " << base::to_underlying(type);
  return base::Version();
}

}  // namespace ash
