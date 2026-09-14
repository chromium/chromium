// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_customization_utils.h"

#include "components/themes/ntp_background_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ntp_customization {

namespace {

constexpr int kMaxDimension = 2556;
constexpr char kAttributionLine1[] = "Attribution Line 1";
constexpr char kAttributionLine2[] = "Attribution Line 2";

SkBitmap CreateTestBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorBLUE);
  return bitmap;
}

void TestGetCustomBackgroundAttributionImpl(const std::string& line_1,
                                            const std::string& line_2,
                                            const std::string& expected);

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

TEST(NtpCustomizationUtilsTest, GetCustomBackgroundAttribution_BothLines) {
  TestGetCustomBackgroundAttributionImpl(
      kAttributionLine1, kAttributionLine2,
      "Attribution Line 1,Attribution Line 2");
}

TEST(NtpCustomizationUtilsTest, GetCustomBackgroundAttribution_Line1Only) {
  TestGetCustomBackgroundAttributionImpl(kAttributionLine1, "",
                                         kAttributionLine1);
}

TEST(NtpCustomizationUtilsTest, GetCustomBackgroundAttribution_Line2Only) {
  TestGetCustomBackgroundAttributionImpl("", kAttributionLine2,
                                         kAttributionLine2);
}

TEST(NtpCustomizationUtilsTest, GetCustomBackgroundAttribution_Empty) {
  TestGetCustomBackgroundAttributionImpl("", "", "");
}

void TestGetCustomBackgroundAttributionImpl(const std::string& line_1,
                                            const std::string& line_2,
                                            const std::string& expected) {
  EXPECT_EQ(GetCustomBackgroundAttribution(line_1, line_2), expected);

  CustomBackground background;
  background.custom_background_attribution_line_1 = line_1;
  background.custom_background_attribution_line_2 = line_2;
  EXPECT_EQ(GetCustomBackgroundAttribution(background), expected);
}

}  // namespace

}  // namespace ntp_customization
