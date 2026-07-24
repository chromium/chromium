// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ntp_customization {

SkBitmap DownsampleImageIfNeeded(const SkBitmap& bitmap, int max_dimension);

constexpr int kMaxDimension = 2556;

static SkBitmap CreateTestBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorBLUE);
  return bitmap;
}

TEST(NtpCustomizationUtilsTest, DownsampleImageIfNeeded_SmallImage) {
  SkBitmap small_bitmap = CreateTestBitmap(500, 500);
  SkBitmap result = DownsampleImageIfNeeded(small_bitmap, kMaxDimension);

  EXPECT_EQ(result.width(), 500);
  EXPECT_EQ(result.height(), 500);
}

TEST(NtpCustomizationUtilsTest,
     DownsampleImageIfNeeded_SlightlyExceedsMaxDimension) {
  // 3000x2000 exceeds 2556, but its halved dimension (1500) is < 2556.
  // Downsampling is not triggered, preserving full resolution sharpness.
  SkBitmap bitmap = CreateTestBitmap(3000, 2000);
  SkBitmap result = DownsampleImageIfNeeded(bitmap, kMaxDimension);

  EXPECT_EQ(result.width(), 3000);
  EXPECT_EQ(result.height(), 2000);
}

TEST(NtpCustomizationUtilsTest, DownsampleImageIfNeeded_MassiveImage) {
  // 6000x4000 has halved width 3000 >= 2556, so sample_size becomes 2.
  SkBitmap large_bitmap = CreateTestBitmap(6000, 4000);
  SkBitmap result = DownsampleImageIfNeeded(large_bitmap, kMaxDimension);

  EXPECT_EQ(result.width(), 3000);
  EXPECT_EQ(result.height(), 2000);
}

TEST(NtpCustomizationUtilsTest, DownsampleImageIfNeeded_PanoramicImage) {
  // 8000x1000 has halved width 4000 >= 2556, so sample_size becomes 2.
  SkBitmap panoramic_bitmap = CreateTestBitmap(8000, 1000);
  SkBitmap result = DownsampleImageIfNeeded(panoramic_bitmap, kMaxDimension);

  EXPECT_EQ(result.width(), 4000);
  EXPECT_EQ(result.height(), 500);
}

}  // namespace ntp_customization
