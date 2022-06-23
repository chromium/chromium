// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_UI_STABILIZER_H_
#define ASH_TEST_ASH_TEST_UI_STABILIZER_H_

#include "ash/wallpaper/test_wallpaper_controller_client.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/icu_test_util.h"

namespace base::subtle {
class ScopedTimeClockOverrides;
}  // namespace base::subtle

namespace gfx {
class Size;
}  // namespace gfx

namespace ash {

// A test helper class that sets up the system UI for pixel tests.
class AshTestUiStabilizer {
 public:
  AshTestUiStabilizer();
  AshTestUiStabilizer(const AshTestUiStabilizer&) = delete;
  AshTestUiStabilizer& operator=(const AshTestUiStabilizer&) = delete;
  ~AshTestUiStabilizer();

  // Makes the variable UI components (such as the battery view and wallpaper)
  // constant to avoid flakiness in pixel tests.
  void StabilizeUi(const gfx::Size& wallpaper_size);

  // Overrides the current time. It ensures that `Time::Now()` is constant.
  void OverrideTime();

  const AccountId& account_id() const { return account_id_; }

 private:
  // Ensures that the system UI is under the dark mode if the dark/light feature
  // is enabled.
  void MaybeSetDarkMode();

  // Sets a pure color wallpaper.
  void SetWallPaper(const gfx::Size& wallpaper_size);

  // Sets the battery state. It ensures that the tray battery icon does not
  // change during pixel tests.
  void SetBatteryState();

  // Used for setting the locale and the time zone.
  const base::test::ScopedRestoreICUDefaultLocale scoped_locale_;
  const base::test::ScopedRestoreDefaultTimezone time_zone_;

  // Overrides the current time.
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_override_;

  const AccountId account_id_;

  // The temporary data directories for wallpaper setting.
  base::ScopedTempDir user_data_dir_;
  base::ScopedTempDir online_wallpaper_dir_;
  base::ScopedTempDir custom_wallpaper_dir_;

  TestWallpaperControllerClient client_;
};

}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_UI_STABILIZER_H_
