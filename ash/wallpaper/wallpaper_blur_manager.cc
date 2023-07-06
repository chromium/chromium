// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_blur_manager.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wallpaper/wallpaper_constants.h"

namespace ash {

namespace {

// Duration of the lock animation performed when pressing a lock button.
constexpr base::TimeDelta kLockAnimationBlurAnimationDuration =
    base::Milliseconds(100);

}  // namespace

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

bool WallpaperBlurManager::UpdateWallpaperBlurForLockState(
    const bool blur,
    const WallpaperType wallpaper_type) {
  if (!IsBlurAllowedForLockState(wallpaper_type)) {
    return false;
  }

  float blur_sigma =
      blur ? wallpaper_constants::kLockLoginBlur : wallpaper_constants::kClear;
  if (wallpaper_type == WallpaperType::kOobe) {
    blur_sigma = wallpaper_constants::kOobeBlur;
  }

  bool changed = is_wallpaper_blurred_for_lock_state_ != blur;
  // Always update the visual wallpaper blur just in case one of the displays is
  // out of sync.
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    changed |=
        root_window_controller->wallpaper_widget_controller()->SetWallpaperBlur(
            blur_sigma, kLockAnimationBlurAnimationDuration);
  }

  is_wallpaper_blurred_for_lock_state_ = blur;

  return changed;
}

void WallpaperBlurManager::RestoreWallpaperBlurForLockState(
    const float blur,
    const WallpaperType wallpaper_type) {
  DCHECK(IsBlurAllowedForLockState(wallpaper_type));
  DCHECK(is_wallpaper_blurred_for_lock_state_);
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    root_window_controller->wallpaper_widget_controller()->SetWallpaperBlur(
        blur, kLockAnimationBlurAnimationDuration);
  }
  is_wallpaper_blurred_for_lock_state_ = false;
}

}  // namespace ash
