// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/surface_set_valuation.h"

#include "chrome/browser/privacy_budget/surface_set_equivalence.h"
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
constexpr auto kSurface3 = blink::IdentifiableSurface::FromTypeAndToken(
    blink::IdentifiableSurface::Type::kWebFeature,
    3);
constexpr auto kSurface4 = blink::IdentifiableSurface::FromTypeAndToken(
    blink::IdentifiableSurface::Type::kWebFeature,
    4);

}  // namespace

TEST(SurfaceSetValuationTest, NoConfig) {
  // Verify behavior when there's no server config that defines costing
  // parameters. All surfaces should be valued at 1.0.

  test::ScopedPrivacyBudgetConfig::Parameters config_params(
      test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling);
  ASSERT_TRUE(config_params.per_surface_cost.empty());
  ASSERT_TRUE(config_params.per_type_cost.empty());
  test::ScopedPrivacyBudgetConfig study_configuration(config_params);

  SurfaceSetEquivalence equivalence;
  SurfaceSetValuation valuation(equivalence);

  IdentifiableSurfaceSet surfaces = {kSurface1, kSurface2, kSurface3,
                                     kSurface4};

  EXPECT_FLOAT_EQ(1.0, valuation.Cost(kSurface1));
  EXPECT_FLOAT_EQ(4.0, valuation.Cost(surfaces));
}

TEST(SurfaceSetValuationTest, PerSurface) {
  test::ScopedPrivacyBudgetConfig::Parameters config_params(
      test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling);
  config_params.per_surface_cost =
      IdentifiableSurfaceCostMap{{kSurface1, 0.5}, {kSurface2, 0.25}};
  test::ScopedPrivacyBudgetConfig study_configuration(config_params);

  SurfaceSetEquivalence equivalence;
  SurfaceSetValuation valuation(equivalence);

  EXPECT_FLOAT_EQ(0.5, valuation.Cost(kSurface1));
  EXPECT_FLOAT_EQ(1.0, valuation.Cost(kSurface4));
  EXPECT_FLOAT_EQ(1.75, valuation.Cost(IdentifiableSurfaceSet{
                            kSurface1, kSurface2, kSurface3}));
}

TEST(SurfaceSetValuationTest, PerType) {
  test::ScopedPrivacyBudgetConfig::Parameters config_params(
      test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling);
  config_params.per_type_cost[blink::IdentifiableSurface::Type::kWebFeature] =
      0.25;
  test::ScopedPrivacyBudgetConfig study_configuration(config_params);

  SurfaceSetEquivalence equivalence;
  SurfaceSetValuation valuation(equivalence);

  EXPECT_FLOAT_EQ(1.0, valuation.Cost(kSurface1));
  EXPECT_FLOAT_EQ(0.25, valuation.Cost(kSurface4));
  EXPECT_FLOAT_EQ(2.25, valuation.Cost(IdentifiableSurfaceSet{
                            kSurface1, kSurface2, kSurface3}));
}

TEST(SurfaceSetValuationTest, ExpectedSurfaceCountForCost) {
  EXPECT_EQ(0u, SurfaceSetValuation::ExpectedSurfaceCountForCost(0.0));
  EXPECT_EQ(1u, SurfaceSetValuation::ExpectedSurfaceCountForCost(0.5));
  EXPECT_EQ(1u, SurfaceSetValuation::ExpectedSurfaceCountForCost(1.0));
  EXPECT_EQ(2u, SurfaceSetValuation::ExpectedSurfaceCountForCost(1.4));
  EXPECT_EQ(3u, SurfaceSetValuation::ExpectedSurfaceCountForCost(2.1));
}
