// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_PIXEL_ASH_PIXEL_DIFFER_H_
#define ASH_TEST_PIXEL_ASH_PIXEL_DIFFER_H_

#include "ash/test/pixel/ash_pixel_diff_util.h"
#include "base/check_op.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"

namespace ash {

// A helper class that provides utility functions for performing pixel diff
// tests via the Skia Gold.
class AshPixelDiffer {
 public:
  // `screenshot_prefix` is the prefix of the screenshot names; `corpus`
  // specifies the result group that will be used to store screenshots in Skia
  // Gold. Read the comment of `SKiaGoldPixelDiff::Init()` for more details.
  AshPixelDiffer(const std::string& screenshot_prefix,
                 const std::string& corpus);
  AshPixelDiffer(const AshPixelDiffer&) = delete;
  AshPixelDiffer& operator=(const AshPixelDiffer&) = delete;
  ~AshPixelDiffer();

  // Takes a full screenshot of the primary display then compares it with the
  // benchmark image specified by the function parameters. Only the pixels
  // within the screen bounds of `ui_components` affect the comparison result.
  // The pixels outside of `ui_components` are blacked out. Returns the
  // comparison result. The function caller has the duty to choose suitable
  // `ui_components` in their tests to avoid unnecessary pixel comparisons.
  // Otherwise, pixel tests could be fragile to the changes in production code.
  //
  // `revision_number` indicates the benchmark image version. `revision_number`
  // and `screenshot_name` collectively specify the benchmark image to compare
  // with. The initial `revision_number` of a new benchmark image should be "0".
  // If there is any code change that updates the benchmark, `revision_number`
  // should increase by 1.
  //
  // `ui_components` is a variadic argument list, consisting of view pointers,
  // widget pointers or window pointers. `ui_components` can have the pointers
  // of different categories. Example usages:
  //
  //  views::View* view_ptr = ...;
  //  views::Widget* widget_ptr = ...;
  //  aura::Window* window_ptr = ...;
  //
  //  CompareUiComponentsOnPrimaryScreen("foo_name1",
  //                                     /*revision_number=*/0,
  //                                     view_ptr);
  //
  //  CompareUiComponentsOnPrimaryScreen("foo_name2",
  //                                     *revision_number=*/0,
  //                                     view_ptr,
  //                                     widget_ptr,
  //                                     window_ptr);
  template <class... UiComponentTypes>
  bool CompareUiComponentsOnPrimaryScreen(const std::string& screenshot_name,
                                          size_t revision_number,
                                          UiComponentTypes... ui_components) {
    DCHECK_GT(sizeof...(ui_components), 0u);
    std::vector<gfx::Rect> rects_in_screen;
    PopulateUiComponentScreenBounds(&rects_in_screen, ui_components...);
    return ComparePrimaryScreenshotInRects(screenshot_name, revision_number,
                                           rects_in_screen);
  }

 private:
  // Compares a screenshot of the primary screen with the specified benchmark
  // image. Only the pixels in `rects_in_screen` affect the comparison result.
  bool ComparePrimaryScreenshotInRects(
      const std::string& screenshot_name,
      size_t revision_number,
      const std::vector<gfx::Rect>& rects_in_screen);

  ui::test::PositiveIfOnlyImageAlgorithm positive_if_only_algorithm_;

  // Used to take screenshots and upload images to the Skia Gold server to
  // perform pixel comparison.
  // NOTE: the user of `ViewSkiaGoldPixelDiff` has the duty to initialize
  // `pixel_diff` before performing any pixel comparison.
  views::ViewSkiaGoldPixelDiff pixel_diff_;
};

}  // namespace ash

#endif  // ASH_TEST_PIXEL_ASH_PIXEL_DIFFER_H_
