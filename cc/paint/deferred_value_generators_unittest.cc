// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/deferred_value_generators.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "cc/paint/deferred_value_map.h"
#include "cc/paint/deferred_values.h"
#include "cc/test/animation_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class MainThreadOnlyGenerator final : public DeferredValueGenerator {
 public:
  Type GetType() const override { return Type::kColor; }
  std::unique_ptr<DeferredValueGenerator> CreateWithSameSource()
      const override {
    NOTREACHED();
  }
  bool HasSameSource(const DeferredValueGenerator&) const override {
    return false;
  }
  bool CanRunOnImplThread() const override { return false; }

 protected:
  void Sample() override { NOTREACHED(); }
};

class DeferredValueGeneratorsTest : public testing::Test {
 protected:
  static constexpr DeferredPropertyKey Key(uint64_t id) {
    return {ElementId(id), DeferredPropertyType::kBackgroundColor};
  }

  static std::unique_ptr<DeferredColorValueGenerator> MakeGenerator(
      scoped_refptr<const AnimationCurveProxy<SkColor4f>> curve =
          base::MakeRefCounted<TestColorCurve>()) {
    return std::make_unique<DeferredColorValueGenerator>(std::move(curve));
  }
};

TEST_F(DeferredValueGeneratorsTest, SetEraseAndClear) {
  DeferredValueGenerators generators;
  generators.Set(Key(1), MakeGenerator());
  ASSERT_TRUE(generators.Find(Key(1)));
  EXPECT_TRUE(generators.Find(Key(1))->CanRunOnImplThread());

  generators.Set(Key(1), std::make_unique<MainThreadOnlyGenerator>());
  EXPECT_FALSE(generators.Find(Key(1))->CanRunOnImplThread());

  generators.Erase(Key(1));
  EXPECT_FALSE(generators.Contains(Key(1)));
  EXPECT_TRUE(generators.empty());

  generators.Set(Key(1), MakeGenerator());
  generators.Clear();
  EXPECT_TRUE(generators.empty());
}

TEST_F(DeferredValueGeneratorsTest, OmitsUnsampledGenerators) {
  auto curve = base::MakeRefCounted<TestColorCurve>();
  DeferredValueGenerators generators;

  auto sampled = MakeGenerator(curve);
  sampled->SetProgress(1.f);
  generators.Set(Key(1), std::move(sampled));

  auto direct = MakeGenerator(curve);
  direct->SetColorForTesting(SkColors::kRed);
  generators.Set(Key(2), std::move(direct));

  generators.Set(Key(3), MakeGenerator(curve));

  DeferredValueMap values;
  EXPECT_EQ((base::flat_set<DeferredPropertyKey>{Key(1), Key(2)}),
            generators.UpdateValues(values));
  EXPECT_EQ(
      DeferredValueMap({{Key(1), SkColors::kWhite}, {Key(2), SkColors::kRed}}),
      values);
  EXPECT_EQ(1, curve->samples());
}

TEST_F(DeferredValueGeneratorsTest, UpdatesInPlaceWhenKeysMatch) {
  DeferredValueGenerators generators;
  generators.Set(Key(1), MakeGenerator());
  generators.Set(Key(2), MakeGenerator());
  generators.SetProgress(Key(1), 1.f);
  generators.SetProgress(Key(2), 0.f);

  DeferredValueMap values;
  std::ignore = generators.UpdateValues(values);
  const auto* storage_before = &*values.colors.begin();

  EXPECT_TRUE(generators.SetProgress(Key(1), 0.5f));
  EXPECT_TRUE(generators.NeedsUpdate());
  EXPECT_EQ(base::flat_set<DeferredPropertyKey>{Key(1)},
            generators.UpdateValues(values));
  EXPECT_FALSE(generators.NeedsUpdate());
  EXPECT_EQ(storage_before, &*values.colors.begin());
  EXPECT_EQ(DeferredValueMap({{Key(1), TestColorCurve::Gray(0.5f)},
                              {Key(2), SkColors::kBlack}}),
            values);
}

TEST_F(DeferredValueGeneratorsTest, ReportsAddedChangedAndRemovedKeys) {
  DeferredValueGenerators generators;
  DeferredValueMap values;

  auto set_solid = [&](DeferredPropertyKey key, SkColor4f color) {
    auto gen =
        MakeGenerator(base::MakeRefCounted<TestColorCurve>(color, color));
    gen->SetProgress(0.5f);
    generators.Set(key, std::move(gen));
  };

  set_solid(Key(1), SkColors::kRed);
  EXPECT_EQ(base::flat_set<DeferredPropertyKey>{Key(1)},
            generators.UpdateValues(values));

  // A new curve that resolves to the same color does not report a change.
  set_solid(Key(1), SkColors::kRed);
  EXPECT_TRUE(generators.UpdateValues(values).empty());

  set_solid(Key(1), SkColors::kBlue);
  EXPECT_EQ(base::flat_set<DeferredPropertyKey>{Key(1)},
            generators.UpdateValues(values));

  generators.Clear();
  EXPECT_EQ(base::flat_set<DeferredPropertyKey>{Key(1)},
            generators.UpdateValues(values));
  EXPECT_TRUE(values.colors.empty());
  EXPECT_TRUE(generators.UpdateValues(values).empty());
}

TEST_F(DeferredValueGeneratorsTest, AddsAndRemovesKeysBetweenKeptKeys) {
  DeferredValueGenerators generators;
  for (uint64_t id = 1; id <= 5; ++id) {
    generators.Set(Key(id), MakeGenerator());
  }
  for (uint64_t id : {1, 3, 5}) {
    generators.SetProgress(Key(id), 1.f);
  }
  DeferredValueMap values;
  std::ignore = generators.UpdateValues(values);

  // Key(3) loses its value while Key(2) and Key(4) gain one.
  generators.SetProgress(Key(3), std::nullopt);
  generators.SetProgress(Key(2), 0.f);
  generators.SetProgress(Key(4), 0.f);
  EXPECT_EQ((base::flat_set<DeferredPropertyKey>{Key(2), Key(3), Key(4)}),
            generators.UpdateValues(values));
  EXPECT_EQ(DeferredValueMap({{Key(1), SkColors::kWhite},
                              {Key(2), SkColors::kBlack},
                              {Key(4), SkColors::kBlack},
                              {Key(5), SkColors::kWhite}}),
            values);
}

TEST_F(DeferredValueGeneratorsTest, SyncFromPreservesMatchingGenerators) {
  auto curve = base::MakeRefCounted<TestColorCurve>();
  DeferredValueGenerators source;
  source.Set(Key(1), MakeGenerator(curve));
  source.Set(Key(2), MakeGenerator(curve));

  DeferredValueGenerators synced;
  EXPECT_FALSE(synced.SyncFrom({}));
  EXPECT_TRUE(synced.SyncFrom(source));
  EXPECT_FALSE(synced.SyncFrom(source));

  // Keys sharing `curve` track progress independently, and re-syncing the same
  // curve keeps their cached samples without resampling.
  synced.SetProgress(Key(1), 1.f);
  synced.SetProgress(Key(2), 0.5f);
  DeferredValueMap values;
  std::ignore = synced.UpdateValues(values);
  EXPECT_EQ(2, curve->samples());

  DeferredValueGenerators next;
  next.Set(Key(1), MakeGenerator(curve));
  next.Set(Key(2), MakeGenerator(curve));
  next.Set(Key(3), MakeGenerator(curve));
  EXPECT_TRUE(synced.SyncFrom(next));
  EXPECT_TRUE(synced.UpdateValues(values).empty());
  EXPECT_EQ(DeferredValueMap({{Key(1), SkColors::kWhite},
                              {Key(2), TestColorCurve::Gray(0.5f)}}),
            values);
  EXPECT_EQ(2, curve->samples());
}

TEST_F(DeferredValueGeneratorsTest,
       SyncFromReplacesChangedAndDropsRemovedKeys) {
  auto curve_a = base::MakeRefCounted<TestColorCurve>();
  auto curve_b =
      base::MakeRefCounted<TestColorCurve>(SkColors::kRed, SkColors::kBlue);

  DeferredValueGenerators initial;
  initial.Set(Key(1), MakeGenerator(curve_a));
  initial.Set(Key(2), MakeGenerator(curve_a));

  DeferredValueGenerators synced;
  synced.SyncFrom(initial);
  synced.SetProgress(Key(1), 1.f);
  synced.SetProgress(Key(2), 1.f);
  DeferredValueMap values;
  std::ignore = synced.UpdateValues(values);

  // Key(1) gets a new unsampled curve and Key(2) is dropped.
  DeferredValueGenerators updated;
  updated.Set(Key(1), MakeGenerator(curve_b));
  EXPECT_TRUE(synced.SyncFrom(updated));
  EXPECT_EQ(1u, synced.size());
  EXPECT_FALSE(synced.Contains(Key(2)));
  EXPECT_EQ((base::flat_set<DeferredPropertyKey>{Key(1), Key(2)}),
            synced.UpdateValues(values));
  EXPECT_TRUE(values.colors.empty());

  synced.SetProgress(Key(1), 1.f);
  EXPECT_EQ(base::flat_set<DeferredPropertyKey>{Key(1)},
            synced.UpdateValues(values));
  EXPECT_EQ(DeferredValueMap({{Key(1), SkColors::kBlue}}), values);
}

TEST_F(DeferredValueGeneratorsTest, SyncFromAddsAndDropsKeysBetweenKeptKeys) {
  auto curve = base::MakeRefCounted<TestColorCurve>();
  DeferredValueGenerators source;
  for (uint64_t id : {1, 3, 5}) {
    source.Set(Key(id), MakeGenerator(curve));
  }
  DeferredValueGenerators synced;
  synced.SyncFrom(source);
  const DeferredValueGenerator* kept = synced.Find(Key(5));

  source.Erase(Key(3));
  EXPECT_TRUE(synced.SyncFrom(source));
  EXPECT_FALSE(synced.Contains(Key(3)));

  source.Set(Key(2), MakeGenerator(curve));
  source.Set(Key(4), MakeGenerator(curve));
  EXPECT_TRUE(synced.SyncFrom(source));
  EXPECT_EQ(4u, synced.size());
  EXPECT_TRUE(synced.Contains(Key(2)));
  EXPECT_TRUE(synced.Contains(Key(4)));

  // Key(5) kept its generator while its neighbors came and went.
  EXPECT_EQ(kept, synced.Find(Key(5)));
  EXPECT_FALSE(synced.SyncFrom(source));
}

TEST_F(DeferredValueGeneratorsTest, SkipsMainThreadOnlyGenerators) {
  DeferredValueGenerators source;
  source.Set(Key(1), MakeGenerator());
  source.Set(Key(2), std::make_unique<MainThreadOnlyGenerator>());

  DeferredValueGenerators synced;
  EXPECT_TRUE(synced.SyncFrom(source));
  EXPECT_EQ(1u, synced.size());
  EXPECT_TRUE(synced.Contains(Key(1)));
  EXPECT_FALSE(synced.Contains(Key(2)));
  EXPECT_FALSE(synced.SyncFrom(source));

  source.Set(Key(1), std::make_unique<MainThreadOnlyGenerator>());
  EXPECT_TRUE(synced.SyncFrom(source));
  EXPECT_TRUE(synced.empty());
  EXPECT_EQ(2u, source.size());
}

}  // namespace
}  // namespace cc
