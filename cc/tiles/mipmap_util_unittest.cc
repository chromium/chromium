// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/tiles/mipmap_util.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace cc {
namespace {

// Ensures that a basic scale works.
TEST(MipMapUtilTest, Basic) {
  const gfx::Size src_size(64, 64);
  const gfx::Size target_size(16, 16);
  const int target_level = 2;
  const SkSize expected_scale = SkSize::Make(0.25f, 0.25f);

  EXPECT_EQ(target_level, MipMapUtil::GetLevelForSize(src_size, target_size));
  EXPECT_SKSIZE_EQ(expected_scale, MipMapUtil::GetScaleAdjustmentForLevel(
                                       src_size, target_level));
  EXPECT_EQ(target_size, MipMapUtil::GetSizeForLevel(src_size, target_level));
  EXPECT_SKSIZE_EQ(expected_scale, MipMapUtil::GetScaleAdjustmentForSize(
                                       src_size, target_size));
}

// Ensures that a no-op scale works.
TEST(MipMapUtilTest, NoScale) {
  const gfx::Size src_size(64, 64);
  const gfx::Size target_size(64, 64);
  const int target_level = 0;
  const SkSize expected_scale = SkSize::Make(1, 1);

  EXPECT_EQ(target_level, MipMapUtil::GetLevelForSize(src_size, target_size));
  EXPECT_SKSIZE_EQ(expected_scale, MipMapUtil::GetScaleAdjustmentForLevel(
                                       src_size, target_level));
  EXPECT_EQ(target_size, MipMapUtil::GetSizeForLevel(src_size, target_level));
  EXPECT_SKSIZE_EQ(expected_scale, MipMapUtil::GetScaleAdjustmentForSize(
                                       src_size, target_size));
}

// Ensures that we return the base mip level if the caller requests an upscale.
TEST(MipMapUtilTest, Upscale) {
  const gfx::Size src_size(64, 64);
  const gfx::Size target_size(128, 128);
  const SkSize result_size = SkSize::Make(1, 1);
  const int result_level = 0;

  EXPECT_EQ(result_level, MipMapUtil::GetLevelForSize(src_size, target_size));
  EXPECT_SKSIZE_EQ(result_size, MipMapUtil::GetScaleAdjustmentForSize(
                                    src_size, target_size));
  EXPECT_EQ(src_size, MipMapUtil::GetSizeForLevel(src_size, result_level));
}

// Ensures that the maximum mip level GetLevelForSize will ever return is 30.
TEST(MipMapUtilTest, MaxMipLevel) {
  int max_mip_level =
      MipMapUtil::GetLevelForSize(gfx::Size(std::numeric_limits<int>::max(),
                                            std::numeric_limits<int>::max()),
                                  gfx::Size(1, 1));
  EXPECT_EQ(max_mip_level, 30);
}

// Ensures that we handle mips of a non-square image correctly (the smaller side
// should never be smaller than 1).
TEST(MipMapUtilTest, NonSquare) {
  const gfx::Size src_size(1024, 1);
  const gfx::Size target_size(64, 1);
  const int target_level = 4;
  const SkSize expected_scale = SkSize::Make(
      static_cast<float>(target_size.width()) / src_size.width(), 1);

  EXPECT_EQ(target_level, MipMapUtil::GetLevelForSize(src_size, target_size));
  EXPECT_SKSIZE_EQ(expected_scale, MipMapUtil::GetScaleAdjustmentForLevel(
                                       src_size, target_level));
  EXPECT_EQ(target_size, MipMapUtil::GetSizeForLevel(src_size, target_level));
  EXPECT_SKSIZE_EQ(expected_scale, MipMapUtil::GetScaleAdjustmentForSize(
                                       src_size, target_size));
}

// Ensures that we handle rounding images correctly.
TEST(MipMapUtilTest, Rounding) {
  const gfx::Size src_size(49, 49);
  const gfx::Size target_size_larger(26, 26);
  const gfx::Size target_size_smaller(25, 25);
  const int target_level_larger = 0;
  const int target_level_smaller = 1;
  const SkSize expected_scale_larger = SkSize::Make(1, 1);
  const SkSize expected_scale_smaller = SkSize::Make(
      static_cast<float>(target_size_smaller.width()) / src_size.width(),
      static_cast<float>(target_size_smaller.height()) / src_size.height());

  EXPECT_EQ(target_level_larger,
            MipMapUtil::GetLevelForSize(src_size, target_size_larger));
  EXPECT_EQ(target_level_smaller,
            MipMapUtil::GetLevelForSize(src_size, target_size_smaller));
  EXPECT_SKSIZE_EQ(
      expected_scale_larger,
      MipMapUtil::GetScaleAdjustmentForLevel(src_size, target_level_larger));
  EXPECT_SKSIZE_EQ(
      expected_scale_smaller,
      MipMapUtil::GetScaleAdjustmentForLevel(src_size, target_level_smaller));
  EXPECT_EQ(src_size,
            MipMapUtil::GetSizeForLevel(src_size, target_level_larger));
  EXPECT_EQ(target_size_smaller,
            MipMapUtil::GetSizeForLevel(src_size, target_level_smaller));
  EXPECT_SKSIZE_EQ(expected_scale_larger, MipMapUtil::GetScaleAdjustmentForSize(
                                              src_size, target_size_larger));
  EXPECT_SKSIZE_EQ(
      expected_scale_smaller,
      MipMapUtil::GetScaleAdjustmentForSize(src_size, target_size_smaller));
}

// Ensures that we round up during mip calculation.
TEST(MipMapUtilTest, RoundUp) {
  const gfx::Size src_sizes[] = {gfx::Size(3, 3), gfx::Size(5, 7),
                                 gfx::Size(11, 14), gfx::Size(17, 31)};
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(MipMapUtil::GetSizeForLevel(src_sizes[i], i + 1),
              gfx::Size(2, 2));
  }
}

}  // namespace
}  // namespace cc
