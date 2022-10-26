// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_PIXEL_ASH_PIXEL_TEST_HELPER_H_
#define ASH_TEST_PIXEL_ASH_PIXEL_TEST_HELPER_H_

#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/wallpaper/test_wallpaper_controller_client.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/icu_test_util.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace ash {

// A test helper class that sets up the system UI for pixel tests.
class AshPixelTestHelper {
 public:
  explicit AshPixelTestHelper(pixel_test::InitParams params);
  AshPixelTestHelper(const AshPixelTestHelper&) = delete;
  AshPixelTestHelper& operator=(const AshPixelTestHelper&) = delete;
  ~AshPixelTestHelper();

  // Makes the variable UI components (such as the battery view and wallpaper)
  // constant to avoid flakiness in pixel tests.
  void StabilizeUi(const gfx::Size& wallpaper_size);

 private:
  // Ensures that the system UI is under the dark mode if the dark/light feature
  // is enabled.
  void MaybeSetDarkMode();

  // Sets a pure color wallpaper.
  void SetWallPaper(const gfx::Size& wallpaper_size);

  // Sets the battery state. It ensures that the tray battery icon does not
  // change during pixel tests.
  void SetBatteryState();

  const pixel_test::InitParams params_;

  // Used for setting the locale and the time zone.
  const base::test::ScopedRestoreICUDefaultLocale scoped_locale_;
  const base::test::ScopedRestoreDefaultTimezone time_zone_;
};

}  // namespace ash

#endif  // ASH_TEST_PIXEL_ASH_PIXEL_TEST_HELPER_H_
