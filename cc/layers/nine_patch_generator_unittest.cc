// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_generator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

TEST(NinePatchGeneratorTest, SetLayoutReturnsChanged) {
  NinePatchGenerator quad_generator;

  EXPECT_TRUE(quad_generator.SetLayout(
      gfx::Size(10, 10), gfx::Size(10, 10), gfx::Rect(1, 1, 8, 8),
      gfx::Rect(1, 1, 2, 2), gfx::Rect(), true, false));

  EXPECT_FALSE(quad_generator.SetLayout(
      gfx::Size(10, 10), gfx::Size(10, 10), gfx::Rect(1, 1, 8, 8),
      gfx::Rect(1, 1, 2, 2), gfx::Rect(), true, false));

  EXPECT_TRUE(quad_generator.SetLayout(
      gfx::Size(10, 10), gfx::Size(10, 10), gfx::Rect(1, 1, 8, 8),
      gfx::Rect(1, 1, 2, 2), gfx::Rect(), false, false));

  EXPECT_FALSE(quad_generator.SetLayout(
      gfx::Size(10, 10), gfx::Size(10, 10), gfx::Rect(1, 1, 8, 8),
      gfx::Rect(1, 1, 2, 2), gfx::Rect(), false, false));

  EXPECT_TRUE(quad_generator.SetLayout(
      gfx::Size(12, 10), gfx::Size(10, 10), gfx::Rect(1, 1, 8, 8),
      gfx::Rect(1, 1, 2, 2), gfx::Rect(), false, false));
}

TEST(NinePatchGeneratorTest, MatchHorizontallyAndVertically) {
  NinePatchGenerator generator;
  generator.SetLayout(gfx::Size(10, 20), gfx::Size(10, 20),
                      gfx::Rect(1, 2, 3, 4), gfx::Rect(1, 2, 7, 16),
                      gfx::Rect(), true, false);

  std::vector<NinePatchGenerator::Patch> patches = generator.GeneratePatches();
  ASSERT_EQ(1u, patches.size());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 20), patches[0].image_rect);
  EXPECT_EQ(gfx::Rect(0, 0, 10, 20), patches[0].output_rect);
  EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f), patches[0].normalized_image_rect);
}

TEST(NinePatchGeneratorTest, MatchHorizontally) {
  NinePatchGenerator generator;
  generator.SetLayout(gfx::Size(10, 20), gfx::Size(10, 30),
                      gfx::Rect(1, 2, 3, 4), gfx::Rect(1, 2, 7, 16),
                      gfx::Rect(), true, false);

  std::vector<NinePatchGenerator::Patch> patches = generator.GeneratePatches();
  ASSERT_EQ(3u, patches.size());

  EXPECT_EQ(gfx::Rect(0, 0, 10, 2), patches[0].image_rect);
  EXPECT_EQ(gfx::Rect(0, 0, 10, 2), patches[0].output_rect);
  EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 0.1f), patches[0].normalized_image_rect);

  EXPECT_EQ(gfx::Rect(0, 2, 10, 4), patches[1].image_rect);
  EXPECT_EQ(gfx::Rect(0, 2, 10, 14), patches[1].output_rect);
  EXPECT_EQ(gfx::RectF(0.f, 0.1f, 1.f, 0.2f), patches[1].normalized_image_rect);

  EXPECT_EQ(gfx::Rect(0, 6, 10, 14), patches[2].image_rect);
  EXPECT_EQ(gfx::Rect(0, 16, 10, 14), patches[2].output_rect);
  EXPECT_EQ(gfx::RectF(0.f, 0.3f, 1.f, 0.7f), patches[2].normalized_image_rect);
}

TEST(NinePatchGeneratorTest, MatchVertically) {
  NinePatchGenerator generator;
  generator.SetLayout(gfx::Size(10, 20), gfx::Size(15, 20),
                      gfx::Rect(1, 2, 3, 4), gfx::Rect(1, 2, 7, 16),
                      gfx::Rect(), true, false);

  std::vector<NinePatchGenerator::Patch> patches = generator.GeneratePatches();
  ASSERT_EQ(3u, patches.size());

  EXPECT_EQ(gfx::Rect(0, 0, 1, 20), patches[0].image_rect);
  EXPECT_EQ(gfx::Rect(0, 0, 1, 20), patches[0].output_rect);
  EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.1f, 1.f), patches[0].normalized_image_rect);

  EXPECT_EQ(gfx::Rect(1, 0, 3, 20), patches[1].image_rect);
  EXPECT_EQ(gfx::Rect(1, 0, 8, 20), patches[1].output_rect);
  EXPECT_EQ(gfx::RectF(0.1f, 0.f, 0.3f, 1.f), patches[1].normalized_image_rect);

  EXPECT_EQ(gfx::Rect(4, 0, 6, 20), patches[2].image_rect);
  EXPECT_EQ(gfx::Rect(9, 0, 6, 20), patches[2].output_rect);
  EXPECT_EQ(gfx::RectF(0.4f, 0.f, 0.6f, 1.f), patches[2].normalized_image_rect);
}

TEST(NinePatchGeneratorTest, GenerateNonSymmetricAperture) {
  NinePatchGenerator quad_generator;

  quad_generator.SetLayout(gfx::Size(10, 10), gfx::Size(10, 10),
                           gfx::Rect(3, 5, 2, 2), gfx::Rect(2, 4, 4, 8),
                           gfx::Rect(), true, false);

  std::vector<NinePatchGenerator::Patch> patches =
      quad_generator.GeneratePatches();

  ASSERT_EQ(9u, patches.size());

  // Center.
  EXPECT_EQ(gfx::Rect(3, 5, 2, 2), patches[8].image_rect);
  EXPECT_EQ(gfx::Rect(2, 4, 6, 2), patches[8].output_rect);
  EXPECT_EQ(gfx::RectF(0.3f, 0.5f, 0.2f, 0.2f),
            patches[8].normalized_image_rect);

  // Bottom.
  EXPECT_EQ(gfx::Rect(3, 7, 2, 3), patches[7].image_rect);
  EXPECT_EQ(gfx::Rect(2, 6, 6, 4), patches[7].output_rect);
  EXPECT_EQ(gfx::RectF(0.3f, 0.7f, 0.2f, 0.3f),
            patches[7].normalized_image_rect);

  // Right.
  EXPECT_EQ(gfx::Rect(5, 5, 5, 2), patches[6].image_rect);
  EXPECT_EQ(gfx::Rect(8, 4, 2, 2), patches[6].output_rect);
  EXPECT_EQ(gfx::RectF(0.5f, 0.5f, 0.5f, 0.2f),
            patches[6].normalized_image_rect);

  // Left.
  EXPECT_EQ(gfx::Rect(0, 5, 3, 2), patches[5].image_rect);
  EXPECT_EQ(gfx::Rect(0, 4, 2, 2), patches[5].output_rect);
  EXPECT_EQ(gfx::RectF(0.f, 0.5f, 0.3f, 0.2f),
            patches[5].normalized_image_rect);

  // Top.
  EXPECT_EQ(gfx::Rect(3, 0, 2, 5), patches[4].image_rect);
  EXPECT_EQ(gfx::Rect(2, 0, 6, 4), patches[4].output_rect);
  EXPECT_EQ(gfx::RectF(0.3f, 0.f, 0.2f, 0.5f),
            patches[4].normalized_image_rect);

  // Bottom-right
  EXPECT_EQ(gfx::Rect(5, 7, 5, 3), patches[3].image_rect);
  EXPECT_EQ(gfx::Rect(8, 6, 2, 4), patches[3].output_rect);
  EXPECT_EQ(gfx::RectF(0.5f, 0.7f, 0.5f, 0.3f),
            patches[3].normalized_image_rect);

  // Bottom-left
  EXPECT_EQ(gfx::Rect(0, 7, 3, 3), patches[2].image_rect);
  EXPECT_EQ(gfx::Rect(0, 6, 2, 4), patches[2].output_rect);
  EXPECT_EQ(gfx::RectF(0.f, 0.7f, 0.3f, 0.3f),
            patches[2].normalized_image_rect);

  // Top-right
  EXPECT_EQ(gfx::Rect(5, 0, 5, 5), patches[1].image_rect);
  EXPECT_EQ(gfx::Rect(8, 0, 2, 4), patches[1].output_rect);
  EXPECT_EQ(gfx::RectF(0.5f, 0.f, 0.5f, 0.5f),
            patches[1].normalized_image_rect);

  // Top-left
  EXPECT_EQ(gfx::Rect(0, 0, 3, 5), patches[0].image_rect);
  EXPECT_EQ(gfx::Rect(0, 0, 2, 4), patches[0].output_rect);
  EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.3f, 0.5f), patches[0].normalized_image_rect);
}

TEST(NinePatchGeneratorTest, LargerLayerGenerate) {
  NinePatchGenerator quad_generator;

  quad_generator.SetLayout(gfx::Size(10, 10), gfx::Size(20, 15),
                           gfx::Rect(1, 1, 8, 8), gfx::Rect(1, 1, 2, 2),
                           gfx::Rect(), true, false);

  std::vector<NinePatchGenerator::Patch> patches =
      quad_generator.GeneratePatches();

  ASSERT_EQ(9u, patches.size());

  // Center.
  EXPECT_EQ(gfx::Rect(1, 1, 8, 8), patches[8].image_rect);
  EXPECT_EQ(gfx::Rect(1, 1, 18, 13), patches[8].output_rect);
  EXPECT_EQ(gfx::RectF(0.1f, 0.1f, 0.8f, 0.8f),
            patches[8].normalized_image_rect);

  // Bottom.
  EXPECT_EQ(gfx::Rect(1, 9, 8, 1), patches[7].image_rect);
  EXPECT_EQ(gfx::Rect(1, 14, 18, 1), patches[7].output_rect);
  EXPECT_EQ(gfx::RectF(0.1f, 0.9f, 0.8f, 0.1f),
            patches[7].normalized_image_rect);

  // Right.
  EXPECT_EQ(gfx::Rect(9, 1, 1, 8), patches[6].image_rect);
  EXPECT_EQ(gfx::Rect(19, 1, 1, 13), patches[6].output_rect);
  EXPECT_EQ(gfx::RectF(0.9f, 0.1f, 0.1f, 0.8f),
            patches[6].normalized_image_rect);

  // Left.
  EXPECT_EQ(gfx::Rect(0, 1, 1, 8), patches[5].image_rect);
  EXPECT_EQ(gfx::Rect(0, 1, 1, 13), patches[5].output_rect);
  EXPECT_EQ(gfx::RectF(0.f, 0.1f, 0.1f, 0.8f),
            patches[5].normalized_image_rect);

  // Top.
  EXPECT_EQ(gfx::Rect(1, 0, 8, 1), patches[4].image_rect);
  EXPECT_EQ(gfx::Rect(1, 0, 18, 1), patches[4].output_rect);
  EXPECT_EQ(gfx::RectF(0.1f, 0.f, 0.8f, 0.1f),
            patches[4].normalized_image_rect);

  // Bottom-right
  EXPECT_EQ(gfx::Rect(9, 9, 1, 1), patches[3].image_rect);
  EXPECT_EQ(gfx::Rect(19, 14, 1, 1), patches[3].output_rect);
  EXPECT_EQ(gfx::RectF(0.9f, 0.9f, 0.1f, 0.1f),
            patches[3].normalized_image_rect);

  // Bottom-left
  EXPECT_EQ(gfx::Rect(0, 9, 1, 1), patches[2].image_rect);
  EXPECT_EQ(gfx::Rect(0, 14, 1, 1), patches[2].output_rect);
  EXPECT_EQ(gfx::RectF(0.f, 0.9f, 0.1f, 0.1f),
            patches[2].normalized_image_rect);

  // Top-right
  EXPECT_EQ(gfx::Rect(9, 0, 1, 1), patches[1].image_rect);
  EXPECT_EQ(gfx::Rect(19, 0, 1, 1), patches[1].output_rect);
  EXPECT_EQ(gfx::RectF(0.9f, 0.f, 0.1f, 0.1f),
            patches[1].normalized_image_rect);

  // Top-left
  EXPECT_EQ(gfx::Rect(0, 0, 1, 1), patches[0].image_rect);
  EXPECT_EQ(gfx::Rect(0, 0, 1, 1), patches[0].output_rect);
  EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.1f, 0.1f), patches[0].normalized_image_rect);
}

}  // namespace
}  // namespace cc
