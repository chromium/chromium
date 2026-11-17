// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_flags.h"

#include "cc/paint/record_paint_canvas.h"
#include "cc/test/paint_op_matchers.h"
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

TEST(PaintFlags, DeferredColor) {
  PaintFlags flags;
  CorePaintFlags& core = flags;
  DeferredPropertyKey key{ElementId(1), DeferredPropertyType::kBackgroundColor};

  // A keyed color is not treated as opaque and skips lite ops / alpha folding.
  flags.setPaintColor({SkColors::kRed, key});
  EXPECT_FALSE(core.isOpaque());
  EXPECT_FALSE(flags.CanConvertToCorePaintFlags());
  EXPECT_FALSE(flags.SupportsFoldingAlpha());
  EXPECT_FALSE(flags.EqualsForTesting(PaintFlags()));
  EXPECT_EQ(core.getPaintColor(), (PaintColor{SkColors::kRed, key}));
  EXPECT_EQ(core.getPaintColor().deferred_key(), key);

  // Overriding alpha flags a main-thread repaint and keeps a 0-alpha draw.
  flags.setAlphaf(0.f);
  EXPECT_FALSE(core.isFullyTransparent());
  EXPECT_FALSE(flags.nothingToDraw());
  EXPECT_EQ(core.getPaintColor(), (PaintColor{{1, 0, 0, 0}, key, true}));

  // Replacing with a fresh PaintColor resets needs_main_thread_repaint.
  flags.setPaintColor({SkColors::kBlue, key});
  EXPECT_EQ(core.getPaintColor(), (PaintColor{SkColors::kBlue, key}));

  // Setting a plain color clears the deferred key and repaint flag.
  core.setColor(SkColors::kBlack);
  EXPECT_EQ(core, CorePaintFlags());
  EXPECT_TRUE(core.isOpaque());
  EXPECT_TRUE(flags.CanConvertToCorePaintFlags());
}

TEST(PaintFlags, DrawArcLiteOpMiterLimit) {
  PaintFlags flags;
  flags.setStrokeMiter(10.f);
  flags.setStyle(PaintFlags::kStroke_Style);
  RecordPaintCanvas c;

  // Open and full-circle arcs have no corners and stay on DrawArcLiteOp.
  c.drawArc({0, 0, 10, 10}, 0, 90, flags);
  flags.setArcClosed(true);
  c.drawArc({0, 0, 10, 10}, 0, 360, flags);
  c.drawArc({0, 0, 10, 10}, 0, -360, flags);

  // A closed partial arc uses DrawArcOp to keep its miter limit.
  c.drawArc({0, 0, 10, 10}, 0, 90, flags);

  EXPECT_THAT(c.ReleaseAsRecord(),
              testing::ElementsAre(
                  PaintOpIs<DrawArcLiteOp>(), PaintOpIs<DrawArcLiteOp>(),
                  PaintOpIs<DrawArcLiteOp>(),
                  PaintOpEq<DrawArcOp>(SkRect{0, 0, 10, 10}, 0, 90, flags)));
}

}  // namespace
}  // namespace cc
