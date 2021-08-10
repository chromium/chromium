// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"

#include "chrome/browser/lens/metrics/lens_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {

TEST(LensRegionSearchControllerTest, UndefinedAspectRatioTest) {
  int height = 0;
  int width = 100;
  EXPECT_EQ(LensRegionSearchController::GetAspectRatioFromSize(height, width),
            LensRegionSearchAspectRatio::UNDEFINED);
}

TEST(LensRegionSearchControllerTest, SquareAspectRatioTest) {
  int height = 100;
  int width = 100;
  EXPECT_EQ(LensRegionSearchController::GetAspectRatioFromSize(height, width),
            LensRegionSearchAspectRatio::SQUARE);
}

TEST(LensRegionSearchControllerTest, WideAspectRatioTest) {
  int height = 100;
  int width = 170;
  EXPECT_EQ(LensRegionSearchController::GetAspectRatioFromSize(height, width),
            LensRegionSearchAspectRatio::WIDE);
}

TEST(LensRegionSearchControllerTest, VeryWideAspectRatioTest) {
  int height = 100;
  int width = 10000;
  EXPECT_EQ(LensRegionSearchController::GetAspectRatioFromSize(height, width),
            LensRegionSearchAspectRatio::VERY_WIDE);
}

TEST(LensRegionSearchControllerTest, TallAspectRatioTest) {
  int height = 170;
  int width = 100;
  EXPECT_EQ(LensRegionSearchController::GetAspectRatioFromSize(height, width),
            LensRegionSearchAspectRatio::TALL);
}

TEST(LensRegionSearchControllerTest, VeryTallAspectRatioTest) {
  int height = 10000;
  int width = 100;
  EXPECT_EQ(LensRegionSearchController::GetAspectRatioFromSize(height, width),
            LensRegionSearchAspectRatio::VERY_TALL);
}

TEST(LensRegionSearchControllerTest, AccurateViewportProportionTest) {
  int screen_height = 1000;
  int screen_width = 1000;
  int image_height = 100;
  int image_width = 100;
  EXPECT_EQ(LensRegionSearchController::CalculateViewportProportionFromAreas(
                screen_height, screen_width, image_width, image_height),
            1);
}

TEST(LensRegionSearchControllerTest, UndefinedViewportProportionTest) {
  int screen_height = 0;
  int screen_width = 0;
  int image_height = 100;
  int image_width = 100;
  EXPECT_EQ(LensRegionSearchController::CalculateViewportProportionFromAreas(
                screen_height, screen_width, image_width, image_height),
            -1);
}

}  // namespace lens
