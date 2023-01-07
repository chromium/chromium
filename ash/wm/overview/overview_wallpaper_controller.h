// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_WALLPAPER_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_WALLPAPER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Class that controls when and how to apply blur and dimming wallpaper upon
// entering and exiting overview mode. Blurs the wallpaper automatically if the
// wallpaper is not visible prior to entering overview mode (covered by a
// window), otherwise animates the blur.
class ASH_EXPORT OverviewWallpaperController : public TabletModeObserver {
 public:
  OverviewWallpaperController();
  OverviewWallpaperController(const OverviewWallpaperController&) = delete;
  OverviewWallpaperController& operator=(const OverviewWallpaperController&) =
      delete;
  ~OverviewWallpaperController() override;

  // There may not be a need to blur or dim the wallpaper for tests.
  static void SetDisableChangeWallpaperForTest(bool disable);

  void Blur(bool animate);
  void Unblur();

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

 private:
  // Called when the wallpaper is to be changed and updates all root windows.
  // Based on the `animate` parameter, several things can happen:
  //   - nullopt: Apply the blur immediately.
  //   - true/false: Animates and applies the blur only if this value matches
  //     whether animations are allowed based on each root window.
  void UpdateWallpaper(bool should_blur, absl::optional<bool> animate);

  // Tracks if the wallpaper blur is applied.
  bool wallpaper_blurred_ = false;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_WALLPAPER_CONTROLLER_H_
