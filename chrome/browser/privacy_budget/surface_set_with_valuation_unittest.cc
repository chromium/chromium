// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/surface_set_with_valuation.h"

#include <cstddef>

#include "base/containers/contains.h"
#include "chrome/browser/privacy_budget/surface_set_equivalence.h"
#include "chrome/browser/privacy_budget/surface_set_valuation.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "chrome/common/privacy_budget/types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {

constexpr auto kSurface1 = blink::IdentifiableSurface::FromTypeAndToken(
    blink::IdentifiableSurface::Type::kCanvasRenderingContext,
    3);
constexpr auto kSurface2 = blink::IdentifiableSurface::FromTypeAndToken(
    blink::IdentifiableSurface::Type::kHTMLMediaElement_CanPlayType,
    5);

// Usual budgets range from 20..40.
constexpr PrivacyBudgetCost kDefaultMediumBudget = 30.0;

struct ScopedValuation {
  ScopedValuation()
      : study_configuration(
            test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling),
        valuation(equivalence) {}

  explicit ScopedValuation(
      test::ScopedPrivacyBudgetConfig::Parameters parameters)
      : study_configuration(parameters), valuation(equivalence) {}

  test::ScopedPrivacyBudgetConfig study_configuration;
  SurfaceSetEquivalence equivalence;
  SurfaceSetValuation valuation;
};

IdentifiableSurfaceSet CreateSurfaceSetWithSize(size_t count) {
  IdentifiableSurfaceSet set;
  set.reserve(count);
  for (auto token = 0u; token < count; ++token) {
    set.insert(blink::IdentifiableSurface::FromTypeAndToken(
        blink::IdentifiableSurface::Type::kWebFeature, token));
  }
  return set;
}

}  // namespace

TEST(SurfaceSetWithValuation, BasicGetters) {
  ScopedValuation fixture;
  SurfaceSetWithValuation set(fixture.valuation);

  EXPECT_TRUE(set.Empty());
  EXPECT_TRUE(set.TryAdd(kSurface1, kDefaultMediumBudget));
  EXPECT_FALSE(set.Empty());
  EXPECT_EQ(1u, set.Size());
  EXPECT_TRUE(set.contains(kSurface1));
  EXPECT_FALSE(set.contains(kSurface2));

  set.Clear();
  EXPECT_TRUE(set.Empty());
}

TEST(SurfaceSetWithValuation, Idempotence) {
  ScopedValuation fixture;
  SurfaceSetWithValuation set(fixture.valuation);

  ASSERT_TRUE(set.TryAdd(kSurface1, kDefaultMediumBudget));
  ASSERT_EQ(SurfaceSetValuation::kDefaultCost, set.Cost());
  ASSERT_EQ(1u, set.Size());

  // Nothing should change.
  ASSERT_TRUE(set.TryAdd(kSurface1, kDefaultMediumBudget));
  ASSERT_EQ(SurfaceSetValuation::kDefaultCost, set.Cost());
}

TEST(SurfaceSetWithValuation, Overflow) {
  ScopedValuation fixture;
  SurfaceSetWithValuation set(fixture.valuation);

  int token = 10;  // Different surfaces just need to have different token
                   // values. The values themselves don't matter.

  // Adds surfaces until we hit the budget ceiling. The only expected outcome of
  // this exercise is that when the loop is done `set` can't accept any more
  // surfaces.
  //
  // By default, ScopedValuation sets things up so that each surface
  // independently costs kDefaultCost.
  for (PrivacyBudgetCost cost = 0.0; cost < kDefaultMediumBudget;
       cost += SurfaceSetValuation::kDefaultCost) {
    ASSERT_TRUE(
        set.TryAdd(blink::IdentifiableSurface::FromTypeAndToken(
                       blink::IdentifiableSurface::Type::kWebFeature, token),
                   kDefaultMediumBudget));
    ++token;
  }

  // Overflows
  EXPECT_FALSE(
      set.TryAdd(blink::IdentifiableSurface::FromTypeAndToken(
                     blink::IdentifiableSurface::Type::kWebFeature, token),
                 kDefaultMediumBudget));
}

TEST(SurfaceSetWithValuation, Fill) {
  ScopedValuation fixture;
  SurfaceSetWithValuation set(fixture.valuation);

  int token = 10;  // As before, this value doesn't matter. All we care about is
                   // that each surface is different.
  while (set.TryAdd(blink::IdentifiableSurface::FromTypeAndToken(
                        blink::IdentifiableSurface::Type::kWebFeature, ++token),
                    kDefaultMediumBudget)) {
  }
  EXPECT_LE(kDefaultMediumBudget, set.Cost());
}

TEST(SurfaceSetWithValuation, AssignWithBudget_Fits) {
  ScopedValuation fixture;
  SurfaceSetWithValuation set(fixture.valuation);

  const auto kRawSurfaceSet = CreateSurfaceSetWithSize(
      kDefaultMediumBudget / SurfaceSetValuation::kDefaultCost);
  auto representative_surface_set =
      fixture.equivalence.GetRepresentatives(kRawSurfaceSet);

  ASSERT_EQ(kRawSurfaceSet.size(), representative_surface_set.size());

  // All the surfaces in representative_surface_set should exist as-is in `set`.
  set.AssignWithBudget(std::move(representative_surface_set),
                       kDefaultMediumBudget);
  ASSERT_EQ(kRawSurfaceSet.size(), set.Size());
}

TEST(SurfaceSetWithValuation, AssignWithBudget_Overflows) {
  ScopedValuation fixture;
  SurfaceSetWithValuation set(fixture.valuation);

  // Contains twice as many surfaces as is expected to fit.
  const auto kRawSurfaceSet = CreateSurfaceSetWithSize(
      2 * kDefaultMediumBudget / SurfaceSetValuation::kDefaultCost);

  auto representative_surface_set =
      fixture.equivalence.GetRepresentatives(kRawSurfaceSet);

  ASSERT_EQ(kRawSurfaceSet.size(), representative_surface_set.size());

  set.AssignWithBudget(std::move(representative_surface_set),
                       kDefaultMediumBudget);
  EXPECT_GT(kRawSurfaceSet.size(), set.Size());
  EXPECT_GE(kDefaultMediumBudget, set.Cost());

  for (const auto& s : set)
    EXPECT_TRUE(base::Contains(kRawSurfaceSet, s.value()));
}

TEST(SurfaceSetWithValuation, RepresentativeSurface) {
  const auto kEquivalenceSet = CreateSurfaceSetWithSize(10);
  const auto kEquivalenceList =
      IdentifiableSurfaceList(kEquivalenceSet.begin(), kEquivalenceSet.end());

  test::ScopedPrivacyBudgetConfig::Parameters pb_parameters(
      test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling);
  pb_parameters.equivalence_classes.emplace_back(kEquivalenceList);

  ScopedValuation fixture(pb_parameters);
  SurfaceSetWithValuation set(fixture.valuation);

  for (const auto& s : kEquivalenceSet) {
    ASSERT_TRUE(set.TryAdd(s, kDefaultMediumBudget));
  }

  // Since all the surfaces are in the same equivalence class, `set` only
  // retains a single representative surface.
  EXPECT_EQ(1u, set.Size());
  for (const auto& s : kEquivalenceSet) {
    EXPECT_TRUE(set.contains(s));
  }
  const auto representative_surface =
      fixture.equivalence.GetRepresentative(*kEquivalenceSet.begin());
  EXPECT_TRUE(set.contains(representative_surface));
}
