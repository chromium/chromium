// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_serialization_history.h"

#include <cstdint>

#include "cc/paint/paint_flags.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_text_property_value.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/test/lottie_test_data.h"
#include "cc/test/paint_image_matchers.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class SkottieSerializationHistoryTest : public ::testing::Test {
 protected:
  static constexpr int64_t kTestPurgePeriod = 2;

  SkottieSerializationHistoryTest() : history_(kTestPurgePeriod) {}

  SkottieSerializationHistory history_;
  SkottieFrameDataMap empty_images;
  SkottieTextPropertyValueMap empty_text_map;
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
  history_.FilterNewSkottieFrameState(*skottie, images, empty_text_map);
  EXPECT_THAT(images, UnorderedElementsAre(
                          SkottieImageIs("asset_a", image_1,
                                         PaintFlags::FilterQuality::kMedium),
                          SkottieImageIs("asset_b", image_2,
                                         PaintFlags::FilterQuality::kMedium)));

  images = {
      {HashSkottieResourceId("asset_a"),
       {image_3, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  history_.FilterNewSkottieFrameState(*skottie, images, empty_text_map);
  EXPECT_THAT(images,
              UnorderedElementsAre(SkottieImageIs(
                  "asset_a", image_3, PaintFlags::FilterQuality::kMedium)));

  images = {
      {HashSkottieResourceId("asset_a"),
       {image_3, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_b"),
       {image_4, PaintFlags::FilterQuality::kMedium}},
  };
  history_.FilterNewSkottieFrameState(*skottie, images, empty_text_map);
  EXPECT_THAT(images,
              UnorderedElementsAre(SkottieImageIs(
                  "asset_b", image_4, PaintFlags::FilterQuality::kMedium)));

  history_.FilterNewSkottieFrameState(*skottie, images, empty_text_map);
  EXPECT_THAT(images, IsEmpty());
}

TEST_F(SkottieSerializationHistoryTest, HandlesEmptyImages) {
  auto skottie = CreateSkottieFromString(
      CreateCustomLottieDataWith2Assets("asset_a", "asset_b"));
  PaintImage blank_image;
  PaintImage image_1 = CreateBitmapImage(gfx::Size(10, 10));
  PaintImage image_2 = CreateBitmapImage(gfx::Size(20, 20));
  ;

  SkottieFrameDataMap images = {
      {HashSkottieResourceId("asset_a"),
       {blank_image, PaintFlags::FilterQuality::kMedium}},
      {HashSkottieResourceId("asset_b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  history_.FilterNewSkottieFrameState(*skottie, images, empty_text_map);
  EXPECT_THAT(images,
              Contains(SkottieImageIs("asset_a", blank_image,
                                      PaintFlags::FilterQuality::kMedium)));

  images = {{HashSkottieResourceId("asset_a"),
             {image_1, PaintFlags::FilterQuality::kMedium}}};
  history_.FilterNewSkottieFrameState(*skottie, images, empty_text_map);
  EXPECT_THAT(images,
              Contains(SkottieImageIs("asset_a", image_1,
                                      PaintFlags::FilterQuality::kMedium)));

  images = {{HashSkottieResourceId("asset_a"),
             {blank_image, PaintFlags::FilterQuality::kMedium}}};
  history_.FilterNewSkottieFrameState(*skottie, images, empty_text_map);
  EXPECT_THAT(images,
              Contains(SkottieImageIs("asset_a", blank_image,
                                      PaintFlags::FilterQuality::kMedium)));

  history_.FilterNewSkottieFrameState(*skottie, images, empty_text_map);
  EXPECT_THAT(images, IsEmpty());
}

TEST_F(SkottieSerializationHistoryTest, FilterNewSkottieFrameText) {
  auto skottie = CreateSkottie(gfx::Size(10, 10), 1);

  SkottieTextPropertyValueMap text_map = {
      {HashSkottieResourceId("node_a"),
       SkottieTextPropertyValue("test_1a", gfx::RectF(1, 1, 1, 1))},
      {HashSkottieResourceId("node_b"),
       SkottieTextPropertyValue("test_1b", gfx::RectF(2, 2, 2, 2))},
  };
  history_.FilterNewSkottieFrameState(*skottie, empty_images, text_map);
  EXPECT_THAT(text_map,
              UnorderedElementsAre(
                  SkottieTextIs("node_a", "test_1a", gfx::RectF(1, 1, 1, 1)),
                  SkottieTextIs("node_b", "test_1b", gfx::RectF(2, 2, 2, 2))));

  text_map = {
      {HashSkottieResourceId("node_a"),
       SkottieTextPropertyValue("test_2a", gfx::RectF(1, 1, 1, 1))},
      {HashSkottieResourceId("node_b"),
       SkottieTextPropertyValue("test_1b", gfx::RectF(2, 2, 2, 2))},
  };
  history_.FilterNewSkottieFrameState(*skottie, empty_images, text_map);
  EXPECT_THAT(text_map, UnorderedElementsAre(SkottieTextIs(
                            "node_a", "test_2a", gfx::RectF(1, 1, 1, 1))));

  text_map = {
      {HashSkottieResourceId("node_a"),
       SkottieTextPropertyValue("test_2a", gfx::RectF(3, 3, 3, 3))},
      {HashSkottieResourceId("node_b"),
       SkottieTextPropertyValue("test_1b", gfx::RectF(2, 2, 2, 2))},
  };
  history_.FilterNewSkottieFrameState(*skottie, empty_images, text_map);
  EXPECT_THAT(text_map, UnorderedElementsAre(SkottieTextIs(
                            "node_a", "test_2a", gfx::RectF(3, 3, 3, 3))));

  history_.FilterNewSkottieFrameState(*skottie, empty_images, text_map);
  EXPECT_THAT(text_map, IsEmpty());
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
  history_.FilterNewSkottieFrameState(*skottie, images, empty_text_map);

  images = {
      {HashSkottieResourceId("asset_a"),
       {image_1, PaintFlags::FilterQuality::kHigh}},
      {HashSkottieResourceId("asset_b"),
       {image_2, PaintFlags::FilterQuality::kMedium}},
  };
  history_.FilterNewSkottieFrameState(*skottie, images, empty_text_map);
  EXPECT_THAT(images,
              UnorderedElementsAre(SkottieImageIs(
                  "asset_a", image_1, PaintFlags::FilterQuality::kHigh)));
}

TEST_F(SkottieSerializationHistoryTest,
       FilterNewSkottieFrameStateMultipleAnimations) {
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
  history_.FilterNewSkottieFrameState(*skottie_1, images_1, empty_text_map);
  history_.FilterNewSkottieFrameState(*skottie_2, images_2, empty_text_map);
  EXPECT_THAT(
      images_2,
      UnorderedElementsAre(SkottieImageIs("asset_2a", image_1,
                                          PaintFlags::FilterQuality::kMedium),
                           SkottieImageIs("asset_2b", image_2,
                                          PaintFlags::FilterQuality::kMedium)));

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
  history_.FilterNewSkottieFrameState(*skottie_1, images_1, empty_text_map);
  history_.FilterNewSkottieFrameState(*skottie_2, images_2, empty_text_map);
  EXPECT_THAT(images_2,
              UnorderedElementsAre(SkottieImageIs(
                  "asset_2a", image_4, PaintFlags::FilterQuality::kMedium)));
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
  history_.FilterNewSkottieFrameState(*skottie_1, images_1, empty_text_map);
  history_.FilterNewSkottieFrameState(*skottie_2, images_2, empty_text_map);

  history_.RequestInactiveAnimationsPurge();
  history_.FilterNewSkottieFrameState(*skottie_1, images_1, empty_text_map);

  // Only |skottie_2| should be purged here since |skottie_1| was updated after
  // the first purge.
  history_.RequestInactiveAnimationsPurge();

  history_.FilterNewSkottieFrameState(*skottie_1, images_1, empty_text_map);
  history_.FilterNewSkottieFrameState(*skottie_2, images_2, empty_text_map);
  EXPECT_THAT(images_1, IsEmpty());
  // History for |skottie_2| should start again.
  EXPECT_THAT(
      images_2,
      UnorderedElementsAre(SkottieImageIs("asset_2a", image_1,
                                          PaintFlags::FilterQuality::kMedium),
                           SkottieImageIs("asset_2b", image_2,
                                          PaintFlags::FilterQuality::kMedium)));
}

}  // namespace
}  // namespace cc
