// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/image_helper.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace explore_sites {

SkColor getBackgroundColor() {
  return SK_ColorTRANSPARENT;
}

const std::vector<unsigned char> kWebpBytes{
    0x52, 0x49, 0x46, 0x46, 0x40, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
    0x56, 0x50, 0x38, 0x58, 0x0a, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x4c, 0x50, 0x48, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x20, 0x18, 0x00, 0x00, 0x00,
    0x30, 0x01, 0x00, 0x9d, 0x01, 0x2a, 0x01, 0x00, 0x01, 0x00, 0x02, 0x00,
    0x34, 0x25, 0xa4, 0x00, 0x03, 0x70, 0x00, 0xfe, 0xfb, 0xfd, 0x50, 0x00,
};

std::vector<unsigned char> kInvalidWebpBytes{
    0x52, 0x49, 0x46, 0x46, 0x40, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42,
    0x50, 0x56, 0x50, 0x38, 0x58, 0x0a, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x4c, 0x50,
};

const int kIconSize = 48;

// Bounds for icon size of 48.
const int kLowerBoundCenter = 11;
const int kUpperBoundCenter = 36;
const int kLowerBoundCorner = 21;
const int kUpperBoundCorner = 26;

class ExploreSitesImageHelperTest : public testing::Test {
 public:
  ExploreSitesImageHelperTest() = default;
  ~ExploreSitesImageHelperTest() override = default;

  EncodedImageList GetEncodedImageList(int num_icons);
  BitmapCallback StoreBitmap() {
    return base::BindLambdaForTesting([&](std::unique_ptr<SkBitmap> bitmap) {
      last_bitmap_list.push_back(std::move(bitmap));
    });
  }

  const base::HistogramTester& histograms() const { return histogram_tester_; }

  std::vector<std::unique_ptr<SkBitmap>> last_bitmap_list;

 protected:
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  base::HistogramTester histogram_tester_;
};

// 1x1 webp image with color value of 0.
EncodedImageList ExploreSitesImageHelperTest::GetEncodedImageList(
    int num_icons) {
  EncodedImageList image_list;
  for (int i = 0; i < num_icons; i++) {
    image_list.push_back(std::make_unique<EncodedImageBytes>(kWebpBytes));
  }
  return image_list;
}

// Test that a single call to get a site icon works.
TEST_F(ExploreSitesImageHelperTest, TestImageHelper_SiteIcon) {
  ImageHelper image_helper;

  image_helper.ComposeSiteImage(StoreBitmap(), GetEncodedImageList(1));

  task_environment_.RunUntilIdle();

  ASSERT_NE(nullptr, last_bitmap_list[0]);
  EXPECT_FALSE(last_bitmap_list[0]->isNull());
  EXPECT_EQ(last_bitmap_list[0]->width(), 1);
  EXPECT_EQ(last_bitmap_list[0]->height(), 1);
  EXPECT_EQ(last_bitmap_list[0]->getColor(0, 0), (unsigned)0);
}

// Test that two sequential calls to get site icons works.
TEST_F(ExploreSitesImageHelperTest, TestImageHelper_SiteIcon_MultipleCalls) {
  ImageHelper image_helper;
  image_helper.ComposeSiteImage(StoreBitmap(), GetEncodedImageList(1));
  task_environment_.RunUntilIdle();

  ASSERT_NE(nullptr, last_bitmap_list[0]);
  EXPECT_FALSE(last_bitmap_list[0]->isNull());

  image_helper.ComposeSiteImage(StoreBitmap(), GetEncodedImageList(1));
  task_environment_.RunUntilIdle();

  ASSERT_NE(nullptr, last_bitmap_list[1]);
  EXPECT_FALSE(last_bitmap_list[1]->isNull());
}

// Test that two concurrent calls to get site icons works.
TEST_F(ExploreSitesImageHelperTest, TestImageHelper_SiteIcon_ConcurrentCalls) {
  ImageHelper image_helper;
  image_helper.ComposeSiteImage(StoreBitmap(), GetEncodedImageList(1));
  image_helper.ComposeSiteImage(StoreBitmap(), GetEncodedImageList(1));
  task_environment_.RunUntilIdle();

  ASSERT_NE(nullptr, last_bitmap_list[0]);
  EXPECT_FALSE(last_bitmap_list[0]->isNull());
  ASSERT_NE(nullptr, last_bitmap_list[1]);
  EXPECT_FALSE(last_bitmap_list[1]->isNull());
}

// Test that call to get category image with one site icon works.
TEST_F(ExploreSitesImageHelperTest, TestImageHelper_CategoryImage_One) {
  ImageHelper image_helper;
  image_helper.ComposeCategoryImage(StoreBitmap(), kIconSize,
                                    GetEncodedImageList(1));
  task_environment_.RunUntilIdle();

  ASSERT_NE(nullptr, last_bitmap_list[0]);
  EXPECT_FALSE(last_bitmap_list[0]->isNull());
  EXPECT_EQ(last_bitmap_list[0]->width(), kIconSize);
  EXPECT_EQ(last_bitmap_list[0]->height(), kIconSize);

  // One square in the center. If inside the bounds, the color should be 0.
  // If outside of the bounds the color should be transparent.
  for (int i = 0; i < last_bitmap_list[0]->width(); i++) {
    for (int j = 0; j < last_bitmap_list[0]->height(); j++) {
      if (j > kLowerBoundCenter && j < kUpperBoundCenter &&
          i > kLowerBoundCenter &&
          i < kUpperBoundCenter) {  // centered square is color 0
        EXPECT_EQ(last_bitmap_list[0]->getColor(j, i), (unsigned)0);
      } else {  // rest of bitmap is transparent
        EXPECT_EQ(last_bitmap_list[0]->getColor(j, i), getBackgroundColor());
      }
    }
  }
}

// Test that call to get category image with two site icons works.
TEST_F(ExploreSitesImageHelperTest, TestImageHelper_CategoryImage_Two) {
  ImageHelper image_helper;
  image_helper.ComposeCategoryImage(StoreBitmap(), kIconSize,
                                    GetEncodedImageList(2));
  task_environment_.RunUntilIdle();

  ASSERT_NE(nullptr, last_bitmap_list[0]);
  EXPECT_FALSE(last_bitmap_list[0]->isNull());
  EXPECT_EQ(last_bitmap_list[0]->width(), kIconSize);
  EXPECT_EQ(last_bitmap_list[0]->height(), kIconSize);

  // Two squares, side by side. If inside the bounds, the color should be 0.
  // If outside of the bounds the color should be transparent.
  for (int i = 0; i < last_bitmap_list[0]->width(); i++) {
    for (int j = 0; j < last_bitmap_list[0]->height(); j++) {
      if ((j < kLowerBoundCorner || j > kUpperBoundCorner) &&
          i > kLowerBoundCenter && i < kUpperBoundCenter) {
        EXPECT_EQ(last_bitmap_list[0]->getColor(j, i), (unsigned)0);
      } else {  // rest of bitmap is transparent
        EXPECT_EQ(last_bitmap_list[0]->getColor(j, i), getBackgroundColor());
      }
    }
  }
}

// Test that call to get category image with three site icons works.
TEST_F(ExploreSitesImageHelperTest, TestImageHelper_CategoryImage_Three) {
  ImageHelper image_helper;
  image_helper.ComposeCategoryImage(StoreBitmap(), kIconSize,
                                    GetEncodedImageList(3));
  task_environment_.RunUntilIdle();

  ASSERT_NE(nullptr, last_bitmap_list[0]);
  EXPECT_FALSE(last_bitmap_list[0]->isNull());
  EXPECT_EQ(last_bitmap_list[0]->width(), kIconSize);
  EXPECT_EQ(last_bitmap_list[0]->height(), kIconSize);

  // Three squares, two on top and one on bottom. If inside the bounds, the
  // color should be 0. If outside of the bounds the color should be
  // transparent.
  for (int i = 0; i < last_bitmap_list[0]->width(); i++) {
    for (int j = 0; j < last_bitmap_list[0]->height(); j++) {
      if ((i < kLowerBoundCorner && j < kLowerBoundCorner) ||  // top left
          (i < kLowerBoundCorner && j > kUpperBoundCorner) ||  // top right
          (i > kUpperBoundCorner && j > kLowerBoundCenter &&
           j < kUpperBoundCenter)) {  // bottom
        EXPECT_EQ(last_bitmap_list[0]->getColor(j, i), (unsigned)0);
      } else {  // rest of bitmap is transparent
        EXPECT_EQ(last_bitmap_list[0]->getColor(j, i), getBackgroundColor());
      }
    }
  }
}

// Test that call to get category image with four site icons works.
TEST_F(ExploreSitesImageHelperTest, TestImageHelper_CategoryImage_Four) {
  ImageHelper image_helper;
  image_helper.ComposeCategoryImage(StoreBitmap(), kIconSize,
                                    GetEncodedImageList(4));
  task_environment_.RunUntilIdle();

  ASSERT_NE(nullptr, last_bitmap_list[0]);
  EXPECT_FALSE(last_bitmap_list[0]->isNull());
  EXPECT_EQ(last_bitmap_list[0]->width(), kIconSize);
  EXPECT_EQ(last_bitmap_list[0]->height(), kIconSize);

  // Four squares in each corner. If inside the bounds, the color should be 0.
  // If outside of the bounds the color should be transparent.
  for (int i = 0; i < last_bitmap_list[0]->width(); i++) {
    for (int j = 0; j < last_bitmap_list[0]->height(); j++) {
      if ((i < kLowerBoundCorner && j < kLowerBoundCorner) ||  // top left
          (i < kLowerBoundCorner && j > kUpperBoundCorner) ||  // top right
          (i > kUpperBoundCorner && j < kLowerBoundCorner) ||  // bottom left
          (i > kUpperBoundCorner && j > kUpperBoundCorner)) {  // bottom right
        EXPECT_EQ(last_bitmap_list[0]->getColor(j, i), (unsigned)0);
      } else {  // rest of bitmap is transparent
        EXPECT_EQ(last_bitmap_list[0]->getColor(j, i), getBackgroundColor());
      }
    }
  }
}

// Test that invalid webp in the image list is ok.
TEST_F(ExploreSitesImageHelperTest, TestImageHelper_CategoryImage_InvalidWebP) {
  ImageHelper image_helper;

  // Try different combinations of invalid and valid webp lists.
  // Trial cases are:
  // [invalid] --> nullptr result
  // [invalid, valid] --> good result
  for (int i = 0; i < 2; i++) {
    EncodedImageList image_list;
    image_list.push_back(
        std::make_unique<EncodedImageBytes>(kInvalidWebpBytes));
    if (i == 1) {
      image_list.push_back(std::make_unique<EncodedImageBytes>(kWebpBytes));
    }
    image_helper.ComposeCategoryImage(StoreBitmap(), kIconSize,
                                      std::move(image_list));

    task_environment_.RunUntilIdle();

    if (i == 0) {
      ASSERT_EQ(nullptr, last_bitmap_list[i]);
    } else {
      ASSERT_NE(nullptr, last_bitmap_list[i]);
      EXPECT_FALSE(last_bitmap_list[i]->isNull());
    }
  }
}

// Test that the ExploreSites.ImageDecoded UMA works.
TEST_F(ExploreSitesImageHelperTest, TestImageHelper_ImageDecodedUMA) {
  ImageHelper image_helper;

  // Record one success UMA from CompseSiteImage.
  image_helper.ComposeSiteImage(StoreBitmap(), GetEncodedImageList(1));
  task_environment_.RunUntilIdle();

  histograms().ExpectTotalCount("ExploreSites.ImageDecoded", 1);
  histograms().ExpectBucketCount("ExploreSites.ImageDecoded", true, 1);

  // Record one failure UMA from ComposeSiteImage.
  EncodedImageList image_list;
  image_list.push_back(std::make_unique<EncodedImageBytes>(kInvalidWebpBytes));
  image_helper.ComposeSiteImage(StoreBitmap(), std::move(image_list));
  task_environment_.RunUntilIdle();

  histograms().ExpectTotalCount("ExploreSites.ImageDecoded", 2);
  histograms().ExpectBucketCount("ExploreSites.ImageDecoded", false, 1);

  // Record 2 samples from ComposeCategoryImage.
  image_helper.ComposeCategoryImage(StoreBitmap(), kIconSize,
                                    GetEncodedImageList(2));
  task_environment_.RunUntilIdle();

  histograms().ExpectTotalCount("ExploreSites.ImageDecoded", 4);
  histograms().ExpectBucketCount("ExploreSites.ImageDecoded", true, 3);
}

}  // namespace explore_sites
