// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_flags.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

constexpr float kEpsilon = 1e-5f;

float ComputeHdrHeadroom(const PaintFlags::DynamicRangeLimitMixture& d,
                         float h) {
  return std::exp2(d.ComputeEffectiveHdrHeadroom(std::log2(h)));
}

TEST(PaintFlags, DynamicRangeLimitSimple) {
  const PaintFlags::DynamicRangeLimitMixture kStandard(
      PaintFlags::DynamicRangeLimit::kStandard);
  const PaintFlags::DynamicRangeLimitMixture kConstrainedHigh(
      PaintFlags::DynamicRangeLimit::kConstrainedHigh);
  const PaintFlags::DynamicRangeLimitMixture kHigh(
      PaintFlags::DynamicRangeLimit::kHigh);

  // Target HDR headroom is 1.
  EXPECT_NEAR(ComputeHdrHeadroom(kStandard, 1.f), 1.f, kEpsilon);
  EXPECT_NEAR(ComputeHdrHeadroom(kConstrainedHigh, 1.f), 1.f, kEpsilon);
  EXPECT_NEAR(ComputeHdrHeadroom(kHigh, 1.f), 1.f, kEpsilon);

  // Target HDR headroom is 2^(1/2). The "high" and "constrained-high" are the
  // same here.
  constexpr float kSqrt2 = 1.4142135623730951f;
  EXPECT_NEAR(ComputeHdrHeadroom(kStandard, kSqrt2), 1.f, kEpsilon);
  EXPECT_NEAR(ComputeHdrHeadroom(kConstrainedHigh, kSqrt2), kSqrt2, kEpsilon);
  EXPECT_NEAR(ComputeHdrHeadroom(kHigh, kSqrt2), kSqrt2, kEpsilon);

  // Target HDR headroom is 4.
  EXPECT_NEAR(ComputeHdrHeadroom(kStandard, 4.f), 1.f, kEpsilon);
  EXPECT_NEAR(ComputeHdrHeadroom(kConstrainedHigh, 4.f), 2.f, kEpsilon);
  EXPECT_NEAR(ComputeHdrHeadroom(kHigh, 4.f), 4.f, kEpsilon);
}

TEST(PaintFlags, DynamicRangeLimitMix) {
  EXPECT_NEAR(
      ComputeHdrHeadroom(PaintFlags::DynamicRangeLimitMixture(1.f, 0.0f), 4.f),
      1.f, kEpsilon);

  EXPECT_NEAR(ComputeHdrHeadroom(
                  PaintFlags::DynamicRangeLimitMixture(0.75f, 0.0f), 4.f),
              1.4142135623730951f, kEpsilon);

  EXPECT_NEAR(
      ComputeHdrHeadroom(PaintFlags::DynamicRangeLimitMixture(0.5f, 0.5f), 4.f),
      1.4142135623730951f, kEpsilon);

  EXPECT_NEAR(ComputeHdrHeadroom(
                  PaintFlags::DynamicRangeLimitMixture(0.25f, 0.5f), 4.f),
              2.f, kEpsilon);

  EXPECT_NEAR(ComputeHdrHeadroom(
                  PaintFlags::DynamicRangeLimitMixture(0.25f, 0.0f), 4.f),
              2.8284271247461903f, kEpsilon);

  EXPECT_NEAR(
      ComputeHdrHeadroom(PaintFlags::DynamicRangeLimitMixture(0.f, 0.5f), 4.f),
      2.8284271247461903f, kEpsilon);

  EXPECT_NEAR(
      ComputeHdrHeadroom(PaintFlags::DynamicRangeLimitMixture(0.f, 0.25f), 4.f),
      3.363585661014858f, kEpsilon);

  EXPECT_NEAR(
      ComputeHdrHeadroom(PaintFlags::DynamicRangeLimitMixture(0.f, 0.f), 4.f),
      4.f, kEpsilon);
}

}  // namespace
}  // namespace cc
