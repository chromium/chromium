// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/sea_pen_utils.h"

#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/size.h"

namespace wallpaper_handlers {
namespace {

using SeaPenUtilsTest = ash::AshTestBase;

TEST_F(SeaPenUtilsTest, GetLargestDisplaySizeSimple) {
  UpdateDisplay("1280x720");
  EXPECT_EQ(gfx::Size(1280, 720), GetLargestDisplaySizeLandscape());
}

TEST_F(SeaPenUtilsTest, GetLargestDisplaySizeRotated) {
  gfx::Size expected(640, 480);

  for (const std::string& display_spec :
       {"640x480/l", "640x480/r", "640x480/u", "480x640"}) {
    UpdateDisplay(display_spec);
    EXPECT_EQ(expected, GetLargestDisplaySizeLandscape()) << display_spec;
  }
}

TEST_F(SeaPenUtilsTest, GetLargestDisplaySizeMultiple) {
  UpdateDisplay("1600x900,1920x1080");
  EXPECT_EQ(gfx::Size(1920, 1080), GetLargestDisplaySizeLandscape());
}

TEST_F(SeaPenUtilsTest, GetLargestDisplaySizeScaleFactor) {
  // The second display is a portrait 4k display with a scale factor of 2.
  // Naively calling display.size() will return {1080,1920}. We still want
  // {3840,2160}.
  UpdateDisplay("2560x1440,3840x2160*2/l");
  EXPECT_EQ(gfx::Size(3840, 2160), GetLargestDisplaySizeLandscape());
}

}  // namespace
}  // namespace wallpaper_handlers
