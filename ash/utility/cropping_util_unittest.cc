// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/cropping_util.h"

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

SkBitmap CreateTestImage(int width, int height) {
  SkBitmap image = gfx::test::CreateBitmap(width, height);
  uint32_t pixel_val = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x, ++pixel_val) {
      uint8_t color = pixel_val % std::numeric_limits<uint8_t>::max();
      *(image.getAddr32(x, y)) = SkColorSetARGB(color, color, color, color);
    }
  }
  return image;
}

}  // namespace

TEST(CroppingUtilTest, CropsLargerPortraitToSmallerLandscape) {
  SkBitmap image = CreateTestImage(600, 1200);
  SkBitmap cropped_actual = CenterCropImage(image, gfx::Size(300, 200));
  // Cropped dimensions: 600 x 400
  SkBitmap cropped_expected;
  ASSERT_TRUE(image.extractSubset(&cropped_expected,
                                  SkIRect::MakeXYWH(0, 400, 600, 400)));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(cropped_actual, cropped_expected));
}

TEST(CroppingUtilTest, CropsLargerLandscapeToSmallerPortrait) {
  SkBitmap image = CreateTestImage(1200, 600);
  SkBitmap cropped_actual = CenterCropImage(image, gfx::Size(200, 300));
  // Cropped dimensions: 400 x 600
  SkBitmap cropped_expected;
  ASSERT_TRUE(image.extractSubset(&cropped_expected,
                                  SkIRect::MakeXYWH(400, 0, 400, 600)));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(cropped_actual, cropped_expected));
}

TEST(CroppingUtilTest, CropsSmallerPortraitToLargerLandscape) {
  SkBitmap image = CreateTestImage(200, 300);
  SkBitmap cropped_actual = CenterCropImage(image, gfx::Size(1200, 600));
  // Cropped dimensions: 200 x 100
  SkBitmap cropped_expected;
  ASSERT_TRUE(image.extractSubset(&cropped_expected,
                                  SkIRect::MakeXYWH(0, 100, 200, 100)));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(cropped_actual, cropped_expected));
}

TEST(CroppingUtilTest, CropsSmallerLandscapeToLargerPortrait) {
  SkBitmap image = CreateTestImage(300, 200);
  SkBitmap cropped_actual = CenterCropImage(image, gfx::Size(600, 1200));
  // Cropped dimensions: 100 x 200
  SkBitmap cropped_expected;
  ASSERT_TRUE(image.extractSubset(&cropped_expected,
                                  SkIRect::MakeXYWH(100, 0, 100, 200)));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(cropped_actual, cropped_expected));
}

TEST(CroppingUtilTest, CropsWithIdenticalAspectRatio) {
  SkBitmap image = CreateTestImage(600, 1200);
  SkBitmap cropped_actual = CenterCropImage(image, gfx::Size(300, 600));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(cropped_actual, image));
}

}  // namespace ash
