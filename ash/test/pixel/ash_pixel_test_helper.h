// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_PIXEL_ASH_PIXEL_TEST_HELPER_H_
#define ASH_TEST_PIXEL_ASH_PIXEL_TEST_HELPER_H_

#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/test/icu_test_util.h"

namespace ash {

class WallpaperController;

// A test helper class that sets up the system UI for pixel tests.
class AshPixelTestHelper : public WallpaperControllerObserver {
 public:
  explicit AshPixelTestHelper(pixel_test::InitParams params);
  AshPixelTestHelper(const AshPixelTestHelper&) = delete;
  AshPixelTestHelper& operator=(const AshPixelTestHelper&) = delete;
  ~AshPixelTestHelper() override;

  // Makes the variable UI components (such as the battery view and wallpaper)
  // constant to avoid flakiness in pixel tests.
  void StabilizeUi();

 private:
  // Ensures that the system UI is under the dark mode if the dark/light feature
  // is enabled.
  void MaybeSetDarkMode();

  // Sets a pure color wallpaper and waits for wallpaper async tasks (resize,
  // color calculation) to complete.
  void SetWallpaper();

  // Sets the battery state. It ensures that the tray battery icon does not
  // change during pixel tests.
  void SetBatteryState();

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;

  const pixel_test::InitParams params_;

  // Allows blocking until wallpaper async tasks have finished and the UI has
  // stabilized. Async tasks include wallpaper resize and color calculation.
  base::OnceClosure on_wallpaper_finalized_;
  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      wallpaper_controller_observation_{this};

  // Used for setting the locale and the time zone.
  const base::test::ScopedRestoreICUDefaultLocale scoped_locale_;
  const base::test::ScopedRestoreDefaultTimezone time_zone_;
};

}  // namespace ash

#endif  // ASH_TEST_PIXEL_ASH_PIXEL_TEST_HELPER_H_
