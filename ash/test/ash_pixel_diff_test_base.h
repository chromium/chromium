// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_PIXEL_DIFF_TEST_BASE_H_
#define ASH_TEST_ASH_PIXEL_DIFF_TEST_BASE_H_

#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/test_wallpaper_controller_client.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/icu_test_util.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"

namespace ash {

// The base class for ash pixel diff tests. This class provides helper functions
// to take screenshots and perform image comparison via the Skia Gold. This
// class also excludes the interference from variable UI components such as the
// time view by setting variables with pre-defined constants.
class AshPixelDiffTestBase : public AshTestBase {
 public:
  // Constructs an AshTestBase with |traits| being forwarded to its
  // TaskEnvironment. MainThreadType always defaults to UI and must not be
  // specified.
  template <typename... TaskEnvironmentTraits>
  NOINLINE explicit AshPixelDiffTestBase(TaskEnvironmentTraits&&... traits)
      : AshPixelDiffTestBase(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI,
            std::forward<TaskEnvironmentTraits>(traits)...)) {}

  // Alternatively a subclass may pass a TaskEnvironment directly.
  explicit AshPixelDiffTestBase(
      std::unique_ptr<base::test::TaskEnvironment> task_environment);

  AshPixelDiffTestBase(const AshPixelDiffTestBase&) = delete;
  AshPixelDiffTestBase& operator=(const AshPixelDiffTestBase&) = delete;
  ~AshPixelDiffTestBase() override;

  // Takes a screenshot of the primary fullscreen then uploads it to the Skia
  // Gold to perform pixel comparison. Returns the comparison result.
  bool ComparePrimaryFullScreen(const std::string& screenshot_name);

  // AshTestBase:
  void SetUp() override;

  views::ViewSkiaGoldPixelDiff* pixel_diff() { return &pixel_diff_; }

 private:
  // Sets a pure color wallpaper.
  void SetWallPaper();

  // Overrides the current time.
  void OverrideTime();

  // Sets the battery state. It ensures that the tray battery icon does not
  // change during pixel tests.
  void SetBatteryState();

  const AccountId kAccountId_;

  // Used for setting the locale and the time zone.
  const base::test::ScopedRestoreICUDefaultLocale scoped_locale_;
  const base::test::ScopedRestoreDefaultTimezone time_zone_;

  // The temporary data directories for wallpaper setting.
  base::ScopedTempDir user_data_dir_;
  base::ScopedTempDir online_wallpaper_dir_;
  base::ScopedTempDir custom_wallpaper_dir_;

  TestWallpaperControllerClient client_;

  // Overrides the current time.
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_override_;

  // Used to take screenshots and upload images to the Skia Gold server to
  // perform pixel comparison.
  // NOTE: the user of `ViewSkiaGoldPixelDiff` has the duty to initialize
  // `pixel_diff` before performing any pixel comparison.
  views::ViewSkiaGoldPixelDiff pixel_diff_;
};

}  // namespace ash

#endif  // ASH_TEST_ASH_PIXEL_DIFF_TEST_BASE_H_
