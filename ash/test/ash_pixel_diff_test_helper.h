// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_PIXEL_DIFF_TEST_HELPER_H_
#define ASH_TEST_ASH_PIXEL_DIFF_TEST_HELPER_H_

#include "ui/views/test/view_skia_gold_pixel_diff.h"

namespace ash {

// A helper class that provides utility functions for performing pixel diff
// tests via the Skia Gold.
class AshPixelDiffTestHelper {
 public:
  // Lists the UI components supported by pixel tests.
  enum class UiComponent {
    // The shelf widget that shows the shelf background.
    kShelfWidget,
  };

  // `screenshot_prefix` is the prefix of the screenshot names; `corpus`
  // specifies the result group that will be used to store screenshots in Skia
  // Gold. Read the comment of `SKiaGoldPixelDiff::Init()` for more details.
  AshPixelDiffTestHelper(const std::string& screenshot_prefix,
                         const std::string& corpus);
  AshPixelDiffTestHelper(const AshPixelDiffTestHelper&) = delete;
  AshPixelDiffTestHelper& operator=(const AshPixelDiffTestHelper&) = delete;
  ~AshPixelDiffTestHelper();

  // Takes a screenshot of the primary fullscreen then uploads it to the Skia
  // Gold to perform pixel comparison. Returns the comparison result.
  // NOTE: use this function only when necessary. Otherwise, a tiny UI change
  // may break many pixel tests.
  bool ComparePrimaryFullScreen(const std::string& screenshot_name);

  // Takes a screenshot of the area associated to `ui_component` then compares
  // it with the benchmark image. Returns the comparison result.
  bool CompareUiComponentScreenshot(const std::string& screenshot_name,
                                    UiComponent ui_component);

  // Compares the screenshot of the area specified by `screen_bounds` with the
  // benchmark image. Returns the comparison result.
  bool ComparePrimaryScreenshotWithBoundsInScreen(
      const std::string& screenshot_name,
      const gfx::Rect& screen_bounds);

 private:
  // Returns the screen bounds of the given UI component.
  // NOTE: this function assumes that the UI component is on the primary screen.
  gfx::Rect GetUiComponentBoundsInScreen(UiComponent ui_component) const;

  // Used to take screenshots and upload images to the Skia Gold server to
  // perform pixel comparison.
  // NOTE: the user of `ViewSkiaGoldPixelDiff` has the duty to initialize
  // `pixel_diff` before performing any pixel comparison.
  views::ViewSkiaGoldPixelDiff pixel_diff_;
};

}  // namespace ash

#endif  // ASH_TEST_ASH_PIXEL_DIFF_TEST_HELPER_H_
