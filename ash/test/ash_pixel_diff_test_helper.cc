// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_pixel_diff_test_helper.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"

namespace ash {

AshPixelDiffTestHelper::AshPixelDiffTestHelper(
    const std::string& screenshot_prefix,
    const std::string& corpus) {
  pixel_diff_.Init(screenshot_prefix, corpus);
}

AshPixelDiffTestHelper::~AshPixelDiffTestHelper() = default;

bool AshPixelDiffTestHelper::ComparePrimaryScreenshotInRects(
    const std::string& screenshot_name,
    const std::vector<gfx::Rect>& rects_in_screen) {
  aura::Window* primary_root_window = Shell::Get()->GetPrimaryRootWindow();
  const aura::WindowTreeHost* host = primary_root_window->GetHost();

  // Handle the case that conversion from screen coordinates to pixel
  // coordinates is not needed.
  if (fabs(host->device_scale_factor() - 1.f) <
      std::numeric_limits<float>::epsilon()) {
    return pixel_diff_.CompareNativeWindowScreenshotInRects(
        screenshot_name, primary_root_window, primary_root_window->bounds(),
        /*algorithm=*/nullptr, rects_in_screen);
  }

  // Convert rects from screen coordinates to pixel coordinates.
  std::vector<gfx::Rect> rects_in_pixel;
  for (const gfx::Rect& screen_bounds : rects_in_screen) {
    gfx::Point top_left = screen_bounds.origin();
    gfx::Point bottom_right = screen_bounds.bottom_right();
    host->ConvertDIPToScreenInPixels(&top_left);
    host->ConvertDIPToScreenInPixels(&bottom_right);
    rects_in_pixel.emplace_back(top_left,
                                gfx::Size(bottom_right.x() - top_left.x(),
                                          bottom_right.y() - top_left.y()));
  }

  return pixel_diff_.CompareNativeWindowScreenshotInRects(
      screenshot_name, primary_root_window, primary_root_window->bounds(),
      /*algorithm=*/nullptr, rects_in_pixel);
}

bool AshPixelDiffTestHelper::ComparePrimaryFullScreen(
    const std::string& screenshot_name) {
  aura::Window* primary_root_window = Shell::Get()->GetPrimaryRootWindow();
  return pixel_diff_.CompareNativeWindowScreenshot(
      screenshot_name, primary_root_window, primary_root_window->bounds());
}

}  // namespace ash
