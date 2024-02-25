// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_BLUR_MANAGER_H_
#define ASH_WALLPAPER_WALLPAPER_BLUR_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/root_window_controller.h"

namespace ash {

// Handles blur state for wallpaper. ChromeOS Wallpaper may be blurred on
// lock/login screen.
class ASH_EXPORT WallpaperBlurManager {
 public:
  WallpaperBlurManager();

  WallpaperBlurManager(const WallpaperBlurManager&) = delete;
  WallpaperBlurManager& operator=(const WallpaperBlurManager&) = delete;

  ~WallpaperBlurManager();

  // Returns whether the current wallpaper is allowed to be blurred on
  // lock/login screen. See https://crbug.com/775591.
  bool IsBlurAllowedForLockState(WallpaperType wallpaper_type) const;

  // Set whether the wallpaper view should blur for lock state. Depending on the
  // order of adding new displays and activating lock state, some displays may
  // or may not be blurred when they should be. Update all of them and return if
  // anything actually changed.
  bool UpdateWallpaperBlurForLockState(bool blur, WallpaperType wallpaper_type);

  // When user presses the physical lock button on device, a quick blur
  // animation shows as the device is locking. This animation may show over
  // other forms of blur. If the user lets go of the lock button before the
  // device is locked, the animation rolls back and should restore the prior
  // blur state.
  void RestoreWallpaperBlurForLockState(float blur,
                                        WallpaperType wallpaper_type);

  bool is_wallpaper_blurred_for_lock_state() const {
    return is_wallpaper_blurred_for_lock_state_;
  }

  bool UpdateBlurForRootWindow(aura::Window* root_window,
                               bool lock_state_changed,
                               bool new_root,
                               WallpaperType wallpaper_type);

  // Make pixel testing more reliable by allowing wallpaper blur.
  void set_allow_blur_for_testing() { allow_blur_for_testing_ = true; }

 private:
  bool allow_blur_for_testing_ = false;
  bool is_wallpaper_blurred_for_lock_state_ = false;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_BLUR_MANAGER_H_
