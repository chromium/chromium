// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/deferred_values.h"

#include <memory>

#include "cc/paint/deferred_property_key.h"
#include "cc/paint/deferred_value_map.h"
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

// CreateWithSameSource() creates a fresh unsampled generator that shares the
// curve without copying progress or cached samples.
TEST(DeferredValuesTest, CreateWithSameSource) {
  auto curve = base::MakeRefCounted<TestColorCurve>();
  DeferredColorValueGenerator generator(curve);
  generator.SetProgress(0.5f);
  generator.RecomputeValue();
  EXPECT_TRUE(generator.has_value());

  std::unique_ptr<DeferredValueGenerator> cloned =
      generator.CreateWithSameSource();
  EXPECT_TRUE(generator.HasSameSource(*cloned));
  EXPECT_TRUE(cloned->CanRunOnImplThread());
  EXPECT_FALSE(cloned->NeedsRecompute());
  EXPECT_FALSE(cloned->has_value());

  DeferredColorValueGenerator other(
      base::MakeRefCounted<TestColorCurve>(SkColors::kRed, SkColors::kBlue));
  EXPECT_FALSE(generator.HasSameSource(other));
}

// CopyFrom() makes the destination equal to `other`, dropping keys that
// `other` does not have.
TEST(DeferredValuesTest, CopyFrom) {
  const DeferredPropertyKey same{ElementId(1),
                                 DeferredPropertyType::kBackgroundColor};
  const DeferredPropertyKey removed{ElementId(2),
                                    DeferredPropertyType::kBackgroundColor};
  const DeferredPropertyKey added{ElementId(3),
                                  DeferredPropertyType::kBackgroundColor};

  DeferredValueMap map({{same, SkColors::kRed}, {added, SkColors::kBlue}});
  DeferredValueMap copied(
      {{same, SkColors::kWhite}, {removed, SkColors::kWhite}});
  copied.CopyFrom(map);
  EXPECT_EQ(map, copied);
  EXPECT_EQ(map, map.Clone());
}

}  // namespace
}  // namespace cc
