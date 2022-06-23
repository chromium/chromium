// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_pixel_diff_test_helper.h"

#include "ash/shell.h"

namespace ash {

AshPixelDiffTestHelper::AshPixelDiffTestHelper() = default;

AshPixelDiffTestHelper::~AshPixelDiffTestHelper() = default;

bool AshPixelDiffTestHelper::ComparePrimaryFullScreen(
    const std::string& screenshot_name) {
  aura::Window* primary_root_window = Shell::Get()->GetPrimaryRootWindow();
  return pixel_diff_.CompareNativeWindowScreenshot(
      screenshot_name, primary_root_window,
      gfx::Rect(primary_root_window->bounds().size()));
}

void AshPixelDiffTestHelper::InitSkiaGoldPixelDiff(
    const std::string& screenshot_prefix,
    const std::string& corpus) {
  pixel_diff_.Init(screenshot_prefix, corpus);
}

}  // namespace ash
