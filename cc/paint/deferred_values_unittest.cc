// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/deferred_values.h"

#include <memory>
#include <utility>

#include "cc/test/animation_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

// SetProgress() waits for RecomputeValue() to sample the curve, so
// intermediate or unchanged updates don't resample.
TEST(DeferredValuesTest, SamplesLazily) {
  auto curve = base::MakeRefCounted<TestColorCurve>();
  DeferredColorValueGenerator generator(curve);
  EXPECT_EQ(DeferredValueGenerator::Type::kColor, generator.GetType());
  EXPECT_FALSE(generator.NeedsRecompute());
  generator.RecomputeValue();
  EXPECT_FALSE(generator.has_value());
  EXPECT_EQ(0, curve->samples());

  // Updates before RecomputeValue() only sample the latest progress.
  EXPECT_TRUE(generator.SetProgress(0.25f));
  EXPECT_TRUE(generator.SetProgress(0.5f));
  EXPECT_TRUE(generator.NeedsRecompute());
  EXPECT_EQ(0, curve->samples());

  generator.RecomputeValue();
  EXPECT_TRUE(generator.has_value());
  EXPECT_EQ(TestColorCurve::Gray(0.5f), generator.GetColor());
  EXPECT_FALSE(generator.NeedsRecompute());
  EXPECT_EQ(1, curve->samples());

  // The same progress doesn't need a new sample.
  EXPECT_FALSE(generator.SetProgress(0.5f));
  EXPECT_FALSE(generator.NeedsRecompute());
  generator.RecomputeValue();
  EXPECT_EQ(1, curve->samples());

  // Clearing progress drops the sampled value so raster falls back.
  EXPECT_TRUE(generator.SetProgress(std::nullopt));
  EXPECT_TRUE(generator.NeedsRecompute());
  generator.RecomputeValue();
  EXPECT_FALSE(generator.has_value());
  EXPECT_FALSE(generator.NeedsRecompute());
  EXPECT_EQ(1, curve->samples());
}

// The map holds sampled and SetColor() values, and skips generators with no
// value so raster uses the color that paint recorded.
TEST(DeferredValuesTest, GenerateDeferredValueMap) {
  auto curve = base::MakeRefCounted<TestColorCurve>();
  const DeferredPropertyKey sampled{ElementId(1),
                                    DeferredPropertyType::kBackgroundColor};
  const DeferredPropertyKey direct{ElementId(2),
                                   DeferredPropertyType::kBackgroundColor};
  const DeferredPropertyKey unsampled{ElementId(3),
                                      DeferredPropertyType::kBackgroundColor};

  DeferredValues values;
  auto sampled_gen = std::make_unique<DeferredColorValueGenerator>(curve);
  sampled_gen->SetProgress(1.f);
  values.emplace(sampled, std::move(sampled_gen));

  auto direct_gen = std::make_unique<DeferredColorValueGenerator>(curve);
  direct_gen->SetColor(SkColors::kRed);
  values.emplace(direct, std::move(direct_gen));

  values.emplace(unsampled,
                 std::make_unique<DeferredColorValueGenerator>(curve));

  DeferredValueMap map = GenerateDeferredValueMap(values);
  EXPECT_EQ(
      DeferredValueMap({{sampled, SkColors::kWhite}, {direct, SkColors::kRed}}),
      map);
  EXPECT_EQ(1, curve->samples());
  EXPECT_EQ(map, map.Clone());
}

}  // namespace
}  // namespace cc
