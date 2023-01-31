// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/wallpaper_types.h"

namespace ash {

bool IsOnlineWallpaper(WallpaperType type) {
  return type == WallpaperType::kDaily || type == WallpaperType::kOnline;
}

}  // namespace ash
