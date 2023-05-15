// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_BLUR_MANAGER_H_
#define ASH_WALLPAPER_WALLPAPER_BLUR_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"

namespace ash {

// Handles blur state for wallpaper. ChromeOS Wallpaper may be blurred on
// login/lock screen, and in window overview mode.
class ASH_EXPORT WallpaperBlurManager {
 public:
  WallpaperBlurManager();

  WallpaperBlurManager(const WallpaperBlurManager&) = delete;
  WallpaperBlurManager& operator=(const WallpaperBlurManager&) = delete;

  ~WallpaperBlurManager();

  // Returns whether the current wallpaper is allowed to be blurred on
  // lock/login screen. See https://crbug.com/775591.
  bool IsBlurAllowedForLockState(const WallpaperType wallpaper_type) const;

  // Make pixel testing more reliable by allowing wallpaper blur.
  void set_allow_blur_for_testing() { allow_blur_for_testing_ = true; }

 private:
  bool allow_blur_for_testing_ = false;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_BLUR_MANAGER_H_
