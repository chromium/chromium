// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/math_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace cc {
namespace {

// TODO(danakj) Move this test to ui/gfx/ when we don't need MathUtil::MapQuad.
TEST(FloatQuadTest, IsRectilinearTest) {
  const int kNumRectilinear = 8;
  gfx::Transform rectilinear_trans[kNumRectilinear];
  rectilinear_trans[1].Rotate(90.f);
  rectilinear_trans[2].Rotate(180.f);
  rectilinear_trans[3].Rotate(270.f);
  rectilinear_trans[4].Skew(0.00000000001f, 0.0f);
  rectilinear_trans[5].Skew(0.0f, 0.00000000001f);
  rectilinear_trans[6].Scale(0.00001f, 0.00001f);
  rectilinear_trans[6].Rotate(180.f);
  rectilinear_trans[7].Scale(100000.f, 100000.f);
  rectilinear_trans[7].Rotate(180.f);

  gfx::QuadF original(
      gfx::RectF(0.01010101f, 0.01010101f, 100.01010101f, 100.01010101f));

  for (int i = 0; i < kNumRectilinear; ++i) {
    bool clipped = false;
    gfx::QuadF quad =
        MathUtil::MapQuad(rectilinear_trans[i], original, &clipped);
    ASSERT_TRUE(!clipped) << "case " << i;
    EXPECT_TRUE(quad.IsRectilinear()) << "case " << i;
  }

  const int kNumNonRectilinear = 10;
  gfx::Transform non_rectilinear_trans[kNumNonRectilinear];
  non_rectilinear_trans[0].Rotate(359.9999f);
  non_rectilinear_trans[1].Rotate(0.0000001f);
  non_rectilinear_trans[2].Rotate(89.9999f);
  non_rectilinear_trans[3].Rotate(90.00001f);
  non_rectilinear_trans[4].Rotate(179.9999f);
  non_rectilinear_trans[5].Rotate(180.00001f);
  non_rectilinear_trans[6].Rotate(269.9999f);
  non_rectilinear_trans[7].Rotate(270.0001f);
  non_rectilinear_trans[8].Skew(0.00001f, 0.0f);
  non_rectilinear_trans[9].Skew(0.0f, 0.00001f);

  for (int i = 0; i < kNumNonRectilinear; ++i) {
    bool clipped = false;
    gfx::QuadF quad =
        MathUtil::MapQuad(non_rectilinear_trans[i], original, &clipped);
    ASSERT_TRUE(!clipped) << "case " << i;
    EXPECT_FALSE(quad.IsRectilinear()) << "case " << i;
  }
}

}  // namespace
}  // namespace cc
