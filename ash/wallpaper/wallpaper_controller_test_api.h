// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_CONTROLLER_TEST_API_H_
#define ASH_WALLPAPER_WALLPAPER_CONTROLLER_TEST_API_H_

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

class WallpaperControllerImpl;

class ASH_EXPORT WallpaperControllerTestApi {
 public:
  explicit WallpaperControllerTestApi(WallpaperControllerImpl* controller);

  WallpaperControllerTestApi(const WallpaperControllerTestApi&) = delete;
  WallpaperControllerTestApi& operator=(const WallpaperControllerTestApi&) =
      delete;

  virtual ~WallpaperControllerTestApi();

  // Creates and sets a new wallpaper that causes the prominent color of the
  // |controller_| to be a valid (i.e. not kInvalidWallpaperColor) color. The
  // WallpaperControllerObservers should be notified as well. This assumes the
  // default DARK-MUTED luma-saturation ranges are in effect.
  //
  // The expected prominent color is returned.
  SkColor ApplyColorProducingWallpaper();

  // Simulates starting the fullscreen wallpaper preview.
  void StartWallpaperPreview();

  // Simulates ending the fullscreen wallpaper preview.
  // |confirm_preview_wallpaper| indicates if the preview wallpaper should be
  // set as the actual user wallpaper.
  void EndWallpaperPreview(bool confirm_preview_wallpaper);

 private:
  WallpaperControllerImpl* controller_;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_CONTROLLER_TEST_API_H_
