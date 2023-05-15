// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_resolution.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using GetMaxDisplaySizeTest = AshTestBase;

TEST_F(GetMaxDisplaySizeTest, DeviceScaleFactorIgnored) {
  // Device scale factor shouldn't affect the native size.
  UpdateDisplay("1000x300*2");
  EXPECT_EQ("1000x300", GetMaxDisplaySizeInNative().ToString());
}

TEST_F(GetMaxDisplaySizeTest, RotatedDisplayReturnsRotatedSize) {
  UpdateDisplay("1000x300*2/r");
  EXPECT_EQ("300x1000", GetMaxDisplaySizeInNative().ToString());
}

TEST_F(GetMaxDisplaySizeTest, UiScalingIgnored) {
  UpdateDisplay("1000x300*2@1.5");
  EXPECT_EQ("1000x300", GetMaxDisplaySizeInNative().ToString());
}

TEST_F(GetMaxDisplaySizeTest, FirstDisplayLarger) {
  UpdateDisplay("400x300,200x100");
  EXPECT_EQ("400x300", GetMaxDisplaySizeInNative().ToString());
}

TEST_F(GetMaxDisplaySizeTest, SecondDisplayLarger) {
  UpdateDisplay("400x300,500x600");
  EXPECT_EQ("500x600", GetMaxDisplaySizeInNative().ToString());
}

TEST_F(GetMaxDisplaySizeTest, MaximumDimensionDifferentDisplays) {
  UpdateDisplay("400x300,100x500");
  EXPECT_EQ("400x500", GetMaxDisplaySizeInNative().ToString());
}

using GetAppropriateResolutionTest = AshTestBase;

TEST_F(GetAppropriateResolutionTest, SmallResolutionIfBothDimensionsSmall) {
  UpdateDisplay(base::StringPrintf("%ix%i", kSmallWallpaperMaxWidth,
                                   kSmallWallpaperMaxHeight));
  EXPECT_EQ(WallpaperResolution::kSmall, GetAppropriateResolution());
}

TEST_F(GetAppropriateResolutionTest, LargeResolutionIfEitherLarge) {
  const std::vector<gfx::Size> sizes = {
      {kSmallWallpaperMaxWidth + 1, kSmallWallpaperMaxHeight},
      {kSmallWallpaperMaxWidth, kSmallWallpaperMaxHeight + 1}};

  for (const auto& size : sizes) {
    UpdateDisplay(base::StringPrintf("%ix%i", size.width(), size.height()));
    EXPECT_EQ(WallpaperResolution::kLarge, GetAppropriateResolution());
  }
}

}  // namespace ash
