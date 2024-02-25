// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/pixel/ash_pixel_differ.h"

#include <string>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/strings/strcat.h"

namespace ash {

AshPixelDiffer::AshPixelDiffer(const std::string& screenshot_prefix,
                               const std::optional<std::string>& corpus)
    : pixel_diff_(screenshot_prefix, corpus) {}

AshPixelDiffer::~AshPixelDiffer() = default;

bool AshPixelDiffer::CompareScreenshotForRootWindowInRects(
    aura::Window* root_window,
    const std::string& screenshot_name,
    size_t revision_number,
    const std::vector<gfx::Rect>& rects_in_screen) {
  // Calculate the full image name incorporating `revision_number`.
  const std::string full_name = base::StrCat(
      {screenshot_name, ".rev_", base::NumberToString(revision_number)});

  const aura::WindowTreeHost* const host = root_window->GetHost();

  // Handle the case that conversion from the root window's coordinates to pixel
  // coordinates is not needed.
  if (fabs(host->device_scale_factor() - 1.f) <
      std::numeric_limits<float>::epsilon()) {
    return pixel_diff_.CompareNativeWindowScreenshotInRects(
        full_name, root_window, root_window->bounds(),
        &positive_if_only_algorithm_, rects_in_screen);
  }

  // Convert rects from screen coordinates to pixel coordinates.
  std::vector<gfx::Rect> rects_in_pixel;
  for (const gfx::Rect& screen_bounds : rects_in_screen) {
    gfx::Point top_left = screen_bounds.origin();
    gfx::Point bottom_right = screen_bounds.bottom_right();
    host->ConvertDIPToPixels(&top_left);
    host->ConvertDIPToPixels(&bottom_right);
    rects_in_pixel.emplace_back(top_left,
                                gfx::Size(bottom_right.x() - top_left.x(),
                                          bottom_right.y() - top_left.y()));
  }

  return pixel_diff_.CompareNativeWindowScreenshotInRects(
      full_name, root_window, root_window->bounds(),
      &positive_if_only_algorithm_, rects_in_pixel);
}

}  // namespace ash
