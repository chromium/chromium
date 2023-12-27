// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_CONTROLLER_TEST_API_H_
#define ASH_WALLPAPER_WALLPAPER_CONTROLLER_TEST_API_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"

namespace ash {

class WallpaperControllerImpl;

class ASH_EXPORT WallpaperControllerTestApi {
 public:
  explicit WallpaperControllerTestApi(WallpaperControllerImpl* controller);

  WallpaperControllerTestApi(const WallpaperControllerTestApi&) = delete;
  WallpaperControllerTestApi& operator=(const WallpaperControllerTestApi&) =
      delete;

  virtual ~WallpaperControllerTestApi();

  // Simulates starting the fullscreen wallpaper preview.
  void StartWallpaperPreview();

  // Simulates ending the fullscreen wallpaper preview.
  // |confirm_preview_wallpaper| indicates if the preview wallpaper should be
  // set as the actual user wallpaper.
  void EndWallpaperPreview(bool confirm_preview_wallpaper);

  // Force a specific set of `calculated_colors` to be set to
  // WallpaperController. Cancels any ongoing requests to calculate wallpaper
  // colors.
  void SetCalculatedColors(const WallpaperCalculatedColors& calculated_colors);

  // Set `calculated_colors` back to nullopt.
  void ResetCalculatedColors();

  // Set a default wallpaper info to simplify testing.
  void SetDefaultWallpaper(const AccountId& account_id);

  // Shows a solid color wallpaper associated with `wallpaper_info`.
  void ShowWallpaperImage(const WallpaperInfo& wallpaper_info,
                          bool preview_mode,
                          bool is_override);

 private:
  raw_ptr<WallpaperControllerImpl, DanglingUntriaged> controller_;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_CONTROLLER_TEST_API_H_
