// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/pixel/ash_pixel_differ.h"

#include <string>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/strings/strcat.h"

namespace ash {
namespace {

// The names of the pixel tests that use the "positive if only" algorithm.
// This list should be removed when all existing tests are migrated.
// NOTE: maintain the array in the alphabetical order.
const std::array<std::string, 5> kMigratedTests = {
    {"AmbientInfoViewTest", "AshNotificationView", "DemoAshPixelDiffTest",
     "LoginShelf", "ScreenCaptureNotification"}};

// Returns true if the test specified by `screenshot_prefix` should use the
// "positive if only" algorithm.
bool ShouldUsePositiveIfOnlyAlgorithm(const std::string& screenshot_prefix) {
  for (const auto& migrated_test : kMigratedTests) {
    if (screenshot_prefix.find(migrated_test) != screenshot_prefix.npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

AshPixelDiffer::AshPixelDiffer(const std::string& screenshot_prefix,
                               const std::string& corpus) {
  pixel_diff_.Init(screenshot_prefix, corpus);
  if (ShouldUsePositiveIfOnlyAlgorithm(screenshot_prefix)) {
    positive_if_only_algorithm_.emplace();
  }
}

AshPixelDiffer::~AshPixelDiffer() = default;

bool AshPixelDiffer::ComparePrimaryScreenshotInRects(
    const std::string& screenshot_name,
    size_t revision_number,
    const std::vector<gfx::Rect>& rects_in_screen) {
  // Calculate the full image name incorporating `revision_number`.
  const std::string full_name = base::StrCat(
      {screenshot_name, ".rev_", base::NumberToString(revision_number)});

  aura::Window* primary_root_window = Shell::Get()->GetPrimaryRootWindow();
  const aura::WindowTreeHost* host = primary_root_window->GetHost();

  // Handle the case that conversion from screen coordinates to pixel
  // coordinates is not needed.
  if (fabs(host->device_scale_factor() - 1.f) <
      std::numeric_limits<float>::epsilon()) {
    return pixel_diff_.CompareNativeWindowScreenshotInRects(
        full_name, primary_root_window, primary_root_window->bounds(),
        positive_if_only_algorithm_ ? &*positive_if_only_algorithm_ : nullptr,
        rects_in_screen);
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
      full_name, primary_root_window, primary_root_window->bounds(),
      positive_if_only_algorithm_ ? &*positive_if_only_algorithm_ : nullptr,
      rects_in_pixel);
}

}  // namespace ash
