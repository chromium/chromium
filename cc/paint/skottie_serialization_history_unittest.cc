// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_serialization_history.h"

#include <cstdint>

#include "cc/paint/paint_flags.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/test/lottie_test_data.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class SkottieSerializationHistoryTest : public ::testing::Test {
 protected:
  static constexpr int64_t kTestPurgePeriod = 2;

  SkottieSerializationHistoryTest() : history_(kTestPurgePeriod) {}

  SkottieSerializationHistory history_;
};

TEST_F(SkottieSerializationHistoryTest, FilterNewSkottieFrameImages) {
  auto skottie = CreateSkottieFromString(
      CreateCustomLottieDataWith2Assets("asset_a", "asset_b"));
  PaintImage image_1 = CreateBitmapImage(gfx::Size(10, 10));
  PaintImage image_2 = CreateBitmapImage(gfx::Size(20, 20));
  PaintImage image_3 = CreateBitmapImage(gfx::Size(30, 30));
  PaintImage image_4 = CreateBitmapImage(gfx::Size(40, 40));

  SkottieFrameDataMap images = {
      {HashSkottieResourceId("asset_a"),
       {image_1, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  history_.FilterNewSkottieFrameImages(*skottie, images);
  EXPECT_THAT(
      images,
      UnorderedElementsAre(
          Pair(HashSkottieResourceId("asset_a"),
               SkottieFrameData({image_1, PaintFlags::FilterQuality::kMedium})),
          Pair(HashSkottieResourceId("asset_b"),
               SkottieFrameData(
                   {image_2, PaintFlags::FilterQuality::kMedium}))));

  images = {
      {HashSkottieResourceId("asset_a"),
       {image_3, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  history_.FilterNewSkottieFrameImages(*skottie, images);
  EXPECT_THAT(
      images,
      UnorderedElementsAre(Pair(
          HashSkottieResourceId("asset_a"),
          SkottieFrameData({image_3, PaintFlags::FilterQuality::kMedium}))));

  images = {
      {HashSkottieResourceId("asset_a"),
       {image_3, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_b"),
       {image_4, PaintFlags::FilterQuality::kMedium}},
  };
  history_.FilterNewSkottieFrameImages(*skottie, images);
  EXPECT_THAT(
      images,
      UnorderedElementsAre(Pair(
          HashSkottieResourceId("asset_b"),
          SkottieFrameData({image_4, PaintFlags::FilterQuality::kMedium}))));

  history_.FilterNewSkottieFrameImages(*skottie, images);
  EXPECT_THAT(images, IsEmpty());
}

TEST_F(SkottieSerializationHistoryTest,
       FilterNewSkottieFrameImagesTakesQualityIntoAccount) {
  auto skottie = CreateSkottieFromString(
      CreateCustomLottieDataWith2Assets("asset_a", "asset_b"));
  PaintImage image_1 = CreateBitmapImage(gfx::Size(10, 10));
  PaintImage image_2 = CreateBitmapImage(gfx::Size(20, 20));

  SkottieFrameDataMap images = {
      {HashSkottieResourceId("asset_a"),
       {image_1, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  history_.FilterNewSkottieFrameImages(*skottie, images);

  images = {
      {HashSkottieResourceId("asset_a"),
       {image_1, PaintFlags::FilterQuality::kHigh}},
      {HashSkottieResourceId("asset_b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  history_.FilterNewSkottieFrameImages(*skottie, images);
  EXPECT_THAT(
      images,
      UnorderedElementsAre(
          Pair(HashSkottieResourceId("asset_a"),
               SkottieFrameData({image_1, PaintFlags::FilterQuality::kHigh}))));
}

TEST_F(SkottieSerializationHistoryTest,
       FilterNewSkottieFrameImagesMultipleAnimations) {
  auto skottie_1 = CreateSkottieFromString(
      CreateCustomLottieDataWith2Assets("asset_1a", "asset_1b"));
  auto skottie_2 = CreateSkottieFromString(
      CreateCustomLottieDataWith2Assets("asset_2a", "asset_2b"));
  PaintImage image_1 = CreateBitmapImage(gfx::Size(10, 10));
  PaintImage image_2 = CreateBitmapImage(gfx::Size(20, 20));
  PaintImage image_3 = CreateBitmapImage(gfx::Size(30, 30));
  PaintImage image_4 = CreateBitmapImage(gfx::Size(40, 40));

  SkottieFrameDataMap images_1 = {
      {HashSkottieResourceId("asset_1a"),
       {image_1, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_1b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  SkottieFrameDataMap images_2 = {
      {HashSkottieResourceId("asset_2a"),
       {image_1, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_2b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  history_.FilterNewSkottieFrameImages(*skottie_1, images_1);
  history_.FilterNewSkottieFrameImages(*skottie_2, images_2);
  EXPECT_THAT(
      images_2,
      UnorderedElementsAre(
          Pair(HashSkottieResourceId("asset_2a"),
               SkottieFrameData({image_1, PaintFlags::FilterQuality::kMedium})),
          Pair(HashSkottieResourceId("asset_2b"),
               SkottieFrameData(
                   {image_2, PaintFlags::FilterQuality::kMedium}))));

  images_1 = {
      {HashSkottieResourceId("asset_1a"),
       {image_3, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_1b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  images_2 = {
      {HashSkottieResourceId("asset_2a"),
       {image_4, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_2b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  history_.FilterNewSkottieFrameImages(*skottie_1, images_1);
  history_.FilterNewSkottieFrameImages(*skottie_2, images_2);
  EXPECT_THAT(
      images_2,
      UnorderedElementsAre(Pair(
          HashSkottieResourceId("asset_2a"),
          SkottieFrameData({image_4, PaintFlags::FilterQuality::kMedium}))));
}

TEST_F(SkottieSerializationHistoryTest, RequestInactiveAnimationsPurge) {
  auto skottie_1 = CreateSkottieFromString(
      CreateCustomLottieDataWith2Assets("asset_1a", "asset_1b"));
  auto skottie_2 = CreateSkottieFromString(
      CreateCustomLottieDataWith2Assets("asset_2a", "asset_2b"));
  PaintImage image_1 = CreateBitmapImage(gfx::Size(10, 10));
  PaintImage image_2 = CreateBitmapImage(gfx::Size(20, 20));

  SkottieFrameDataMap images_1 = {
      {HashSkottieResourceId("asset_1a"),
       {image_1, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_1b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  SkottieFrameDataMap images_2 = {
      {HashSkottieResourceId("asset_2a"),
       {image_1, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_2b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  history_.FilterNewSkottieFrameImages(*skottie_1, images_1);
  history_.FilterNewSkottieFrameImages(*skottie_2, images_2);

  history_.RequestInactiveAnimationsPurge();
  history_.FilterNewSkottieFrameImages(*skottie_1, images_1);

  // Only |skottie_2| should be purged here since |skottie_1| was updated after
  // the first purge.
  history_.RequestInactiveAnimationsPurge();

  history_.FilterNewSkottieFrameImages(*skottie_1, images_1);
  history_.FilterNewSkottieFrameImages(*skottie_2, images_2);
  EXPECT_THAT(images_1, IsEmpty());
  // History for |skottie_2| should start again.
  EXPECT_THAT(
      images_2,
      UnorderedElementsAre(
          Pair(HashSkottieResourceId("asset_2a"),
               SkottieFrameData({image_1, PaintFlags::FilterQuality::kMedium})),
          Pair(HashSkottieResourceId("asset_2b"),
               SkottieFrameData(
                   {image_2, PaintFlags::FilterQuality::kMedium}))));
}

}  // namespace
}  // namespace cc
