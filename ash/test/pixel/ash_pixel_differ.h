// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_PIXEL_ASH_PIXEL_DIFFER_H_
#define ASH_TEST_PIXEL_ASH_PIXEL_DIFFER_H_

#include <iterator>
#include <optional>

#include "ash/shell.h"
#include "ash/test/pixel/ash_pixel_diff_util.h"
#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"

namespace ash {

// A helper class that provides utility functions for performing pixel diff
// tests via the Skia Gold.
class AshPixelDiffer {
 public:
  // `screenshot_prefix` is the prefix of the screenshot names; `corpus`
  // specifies the result group that will be used to store screenshots in Skia
  // Gold. Read the comment of `SKiaGoldPixelDiff::GetSession()` for more
  // details.
  explicit AshPixelDiffer(const std::string& screenshot_prefix,
                          const std::optional<std::string>& corpus = {});
  AshPixelDiffer(const AshPixelDiffer&) = delete;
  AshPixelDiffer& operator=(const AshPixelDiffer&) = delete;
  ~AshPixelDiffer();

  // Takes a full screenshot of `root_window` then compares it with the
  // benchmark image specified by the function parameters. Only the pixels
  // within the screen bounds of `ui_components` affect the comparison result.
  // The pixels outside of `ui_components` are blacked out. Returns the
  // comparison result. The function caller has the duty to choose suitable
  // `ui_components` in their tests to avoid unnecessary pixel comparisons.
  // Otherwise, pixel tests could be fragile to the changes in production code.
  //
  // Checks that `root_window` is not null and is actually a root window.
  //
  // `revision_number` indicates the benchmark image version. `revision_number`
  // and `screenshot_name` collectively specify the benchmark image to compare
  // with. The initial `revision_number` of a new benchmark image should be "0".
  // If there is any code change that updates the benchmark, `revision_number`
  // should increase by 1.
  //
  // `ui_components` is a variadic argument list, consisting of view pointers,
  // widget pointers or window pointers. `ui_components` can have the pointers
  // of different categories.
  //
  // Note that there exist two convenience functions,
  // `CompareUiComponentsOnPrimaryScreen()` and
  // `CompareUiComponentsOnSecondaryScreen()`, for comparing the UI components
  // against the primary or secondary display's root window, respectively. They
  // can be used in a similar way to the example below, with the difference of
  // not having to provide a particular `root_window`.
  //
  // Example usages (of `CompareUiComponentsOnRootWindow()`):
  //
  //  aura::Window* root_window = ...;
  //  views::View* view_ptr = ...;
  //  views::Widget* widget_ptr = ...;
  //  aura::Window* window_ptr = ...;
  //
  //  CompareUiComponentsOnRootWindow(root_window,
  //                                  "foo_name1",
  //                                  /*revision_number=*/0,
  //                                  view_ptr);
  //
  //  CompareUiComponentsOnRootWindow(root_window,
  //                                  "foo_name2",
  //                                  /*revision_number=*/0,
  //                                  view_ptr,
  //                                  widget_ptr,
  //                                  window_ptr);
  template <class... UiComponentTypes>
  bool CompareUiComponentsOnRootWindow(aura::Window* const root_window,
                                       const std::string& screenshot_name,
                                       size_t revision_number,
                                       UiComponentTypes... ui_components) {
    CHECK(root_window && root_window->IsRootWindow());
    CHECK_GT(sizeof...(ui_components), 0u);
    std::vector<gfx::Rect> rects_in_screen;
    PopulateUiComponentScreenBounds(&rects_in_screen, ui_components...);

    // Adjust the UI component bounds to be relative to the origin of the
    // specified root window.
    std::vector<gfx::Rect> adjusted_rects_in_screen;
    const gfx::Rect root_window_bounds = root_window->GetBoundsInScreen();
    base::ranges::transform(
        rects_in_screen, std::back_inserter(adjusted_rects_in_screen),
        [&root_window_bounds](const gfx::Rect& rect) {
          gfx::Rect adjusted_rect(rect);
          adjusted_rect.Offset(-root_window_bounds.OffsetFromOrigin());
          return adjusted_rect;
        });

    return CompareScreenshotForRootWindowInRects(root_window, screenshot_name,
                                                 revision_number,
                                                 adjusted_rects_in_screen);
  }

  // Like `CompareUiComponentsOnRootWindow()` but forces the screenshot to be of
  // the primary display's root window.
  //
  // TODO(b/286916355): Rename this function.
  template <class... UiComponentTypes>
  bool CompareUiComponentsOnPrimaryScreen(const std::string& screenshot_name,
                                          size_t revision_number,
                                          UiComponentTypes... ui_components) {
    return CompareUiComponentsOnRootWindow(Shell::GetPrimaryRootWindow(),
                                           screenshot_name, revision_number,
                                           ui_components...);
  }

  // Like `CompareUiComponentsOnRootWindow()` but forces the screenshot to be of
  // the secondary display's root window. There should be exactly two displays
  // present when using this function; if there are more than two displays then
  // consider using `CompareUiComponentsOnRootWindow()` instead to ensure the
  // proper root window is used for the comparison.
  //
  // TODO(b/286916355): Rename this function.
  template <class... UiComponentTypes>
  bool CompareUiComponentsOnSecondaryScreen(const std::string& screenshot_name,
                                            size_t revision_number,
                                            UiComponentTypes... ui_components) {
    CHECK_EQ(Shell::GetAllRootWindows().size(), 2u);
    const display::Display& display =
        display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
            .GetSecondaryDisplay();
    return CompareUiComponentsOnRootWindow(
        Shell::GetRootWindowForDisplayId(display.id()), screenshot_name,
        revision_number, ui_components...);
  }

 private:
  // Compares a screenshot of `root_window` with the specified benchmark image.
  // Only the pixels in `rects_in_screen` affect the comparison result.
  bool CompareScreenshotForRootWindowInRects(
      aura::Window* const root_window,
      const std::string& screenshot_name,
      size_t revision_number,
      const std::vector<gfx::Rect>& rects_in_screen);

  ui::test::PositiveIfOnlyImageAlgorithm positive_if_only_algorithm_;

  // Used to take screenshots and upload images to the Skia Gold server to
  // perform pixel comparison.
  views::ViewSkiaGoldPixelDiff pixel_diff_;
};

}  // namespace ash

#endif  // ASH_TEST_PIXEL_ASH_PIXEL_DIFFER_H_
