// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_resizer.h"

#include <memory>

#include "base/logging.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/lottie/animation.h"
#include "ui/views/border.h"
#include "ui/views/controls/animated_image_view.h"

namespace ash {
namespace {

using ::testing::Eq;

std::unique_ptr<views::AnimatedImageView> CreateAnimatedImageView(
    const gfx::Size& animation_size,
    const gfx::Rect& view_bounds) {
  auto view = std::make_unique<views::AnimatedImageView>();
  view->SetAnimatedImage(std::make_unique<lottie::Animation>(
      cc::CreateSkottie(animation_size,
                        /*duration_secs=*/1)));
  view->SetBoundsRect(view_bounds);
  return view;
}

}  // namespace

TEST(AmbientAnimationResizerTest, LandscapeScalesDownWidthAndCropsHeight) {
  auto view =
      CreateAnimatedImageView(gfx::Size(2000, 1500), gfx::Rect(1000, 600));
  AmbientAnimationResizer::Resize(*view);
  EXPECT_THAT(view->GetImageBounds(), Eq(gfx::Rect(0, -75, 1000, 750)));
}

TEST(AmbientAnimationResizerTest,
     LandscapeScalesDownWidthAndCropsHeightWithInsets) {
  auto view =
      CreateAnimatedImageView(gfx::Size(2000, 1500), gfx::Rect(1000, 600));
  // Content bounds are 500 x 300.
  view->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(150, 250)));
  AmbientAnimationResizer::Resize(*view);
  EXPECT_TRUE(view->GetImageBounds().ApproximatelyEqual(
      gfx::Rect(250, 150 - (75 / 2), 500, 375), /*tolerance=*/1));
}

TEST(AmbientAnimationResizerTest, LandscapeScalesDownWidthAndHeight) {
  auto view =
      CreateAnimatedImageView(gfx::Size(2000, 1500), gfx::Rect(1000, 750));
  AmbientAnimationResizer::Resize(*view);
  EXPECT_THAT(view->GetImageBounds(), Eq(gfx::Rect(0, 0, 1000, 750)));
}

TEST(AmbientAnimationResizerTest,
     LandscapeScalesDownWidthAndDoesNotCropHeight) {
  auto view =
      CreateAnimatedImageView(gfx::Size(2000, 1500), gfx::Rect(1000, 800));
  AmbientAnimationResizer::Resize(*view);
  EXPECT_THAT(view->GetImageBounds(), Eq(gfx::Rect(0, 25, 1000, 750)));
}

TEST(AmbientAnimationResizerTest, LandscapeScalesUpByWidth) {
  auto view =
      CreateAnimatedImageView(gfx::Size(2000, 1500), gfx::Rect(2500, 1500));
  AmbientAnimationResizer::Resize(*view);
  EXPECT_TRUE(view->GetImageBounds().ApproximatelyEqual(
      gfx::Rect(0, -(375 / 2), 2500, 1875), /*tolerance=*/1));
}

TEST(AmbientAnimationResizerTest, LandscapeAppliesJitter) {
  auto view =
      CreateAnimatedImageView(gfx::Size(2000, 1500), gfx::Rect(1000, 600));
  AmbientAnimationResizer::Resize(*view, /*padding_for_jitter=*/10);
  // New Height: 1500 * (1020 / 2000) = 765
  // New Y origin: -(765 - 600) / 2 = 82.5
  EXPECT_THAT(view->GetImageBounds(), Eq(gfx::Rect(-10, -82, 1020, 765)));
}

TEST(AmbientAnimationResizerTest, PortraitScalesDownWidthAndCropsHeight) {
  auto view =
      CreateAnimatedImageView(gfx::Size(1500, 2000), gfx::Rect(750, 1200));
  AmbientAnimationResizer::Resize(*view);
  EXPECT_THAT(view->GetImageBounds(), Eq(gfx::Rect(0, 100, 750, 1000)));
}

TEST(AmbientAnimationResizerTest,
     PortraitScalesDownWidthAndCropsHeightWithInsets) {
  auto view =
      CreateAnimatedImageView(gfx::Size(1500, 2000), gfx::Rect(600, 1000));
  // Content bounds are 300 x 500.
  view->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(250, 150)));
  AmbientAnimationResizer::Resize(*view);
  EXPECT_TRUE(view->GetImageBounds().ApproximatelyEqual(
      gfx::Rect(150, 250 + (500 - 400) / 2, 300, 400), /*tolerance=*/1));
}

TEST(AmbientAnimationResizerTest, PortraitScalesDownWidthAndHeight) {
  auto view =
      CreateAnimatedImageView(gfx::Size(1500, 2000), gfx::Rect(750, 1000));
  AmbientAnimationResizer::Resize(*view);
  EXPECT_THAT(view->GetImageBounds(), Eq(gfx::Rect(0, 0, 750, 1000)));
}

TEST(AmbientAnimationResizerTest, PortraitScalesDownWidthAndDoesNotCropHeight) {
  auto view =
      CreateAnimatedImageView(gfx::Size(1500, 2000), gfx::Rect(600, 1000));
  AmbientAnimationResizer::Resize(*view);
  // Scaled height = 2000 / (1500 / 600) = 800
  // Image y offset = (1000 - 800) / 2 = 100
  EXPECT_THAT(view->GetImageBounds(), Eq(gfx::Rect(0, 100, 600, 800)));
}

TEST(AmbientAnimationResizerTest, PortraitScalesUpByWidth) {
  auto view =
      CreateAnimatedImageView(gfx::Size(1500, 2000), gfx::Rect(1800, 2000));
  AmbientAnimationResizer::Resize(*view);
  EXPECT_TRUE(view->GetImageBounds().ApproximatelyEqual(
      gfx::Rect(0, -200, 1800, 2400), /*tolerance=*/1));
}

TEST(AmbientAnimationResizerTest, PortraitAppliesJitter) {
  auto view =
      CreateAnimatedImageView(gfx::Size(1500, 2000), gfx::Rect(600, 1000));
  AmbientAnimationResizer::Resize(*view, /*padding_for_jitter=*/10);
  // New Height: 2000 * (620 / 1500) = 826 2/3
  // New Y origin: -(1000 - 827) / 2 = 86.5
  EXPECT_TRUE(view->GetImageBounds().ApproximatelyEqual(
      gfx::Rect(-10, 87, 620, 827), /*tolerance=*/1));
}

}  // namespace ash
