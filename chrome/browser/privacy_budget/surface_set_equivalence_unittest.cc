// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/surface_set_equivalence.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr auto kSurface1 = blink::IdentifiableSurface::FromMetricHash(100);
constexpr auto kSurface2 = blink::IdentifiableSurface::FromMetricHash(257);
constexpr auto kSurface3 = blink::IdentifiableSurface::FromMetricHash(300);
constexpr auto kSurface4 = blink::IdentifiableSurface::FromMetricHash(301);

}  // namespace

TEST(SurfaceSetEquivalenceTest, NoEquivalenceSets) {
  base::FieldTrialParams params = {
      {features::kIdentifiabilityStudySurfaceEquivalenceClasses.name, ""}};
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kIdentifiabilityStudy,
                                              params);
  SurfaceSetEquivalence equivalence;

  // In the absence of any equivalence sets,
  // `SurfaceSetEquivalence.GetRepresentative()` is an identity function.
  EXPECT_EQ(kSurface1, equivalence.GetRepresentative(kSurface1).value());

  // Ditto for `GetRepresentatives()` with the exception that it clones the
  // input.
  IdentifiableSurfaceSet surfaces = {kSurface1, kSurface2, kSurface3};
  EXPECT_EQ(3u, equivalence.GetRepresentatives(surfaces).size());
}

TEST(SurfaceSetEquivalenceTest, EmptyCollections) {
  base::FieldTrialParams params = {
      {features::kIdentifiabilityStudySurfaceEquivalenceClasses.name, ""}};
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kIdentifiabilityStudy,
                                              params);
  SurfaceSetEquivalence equivalence;

  EXPECT_TRUE(equivalence.GetRepresentatives(IdentifiableSurfaceSet{}).empty());
  EXPECT_TRUE(
      equivalence.GetRepresentatives(IdentifiableSurfaceList{}).empty());
}

TEST(SurfaceSetEquivalenceTest, SingleEquivalenceSet) {
  // kSurface1 and kSurface2 are in an equivalence set. kSurface1 is the
  // representative.
  base::FieldTrialParams params = {
      {features::kIdentifiabilityStudySurfaceEquivalenceClasses.name,
       "100;257"}};
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kIdentifiabilityStudy,
                                              params);
  SurfaceSetEquivalence equivalence;

  EXPECT_EQ(kSurface1, equivalence.GetRepresentative(kSurface2).value());

  IdentifiableSurfaceList surface_list = {kSurface1, kSurface2, kSurface3};
  IdentifiableSurfaceSet surface_set(surface_list.begin(), surface_list.end());
  RepresentativeSurfaceList expected_surface_list = {
      RepresentativeSurface(kSurface1), RepresentativeSurface(kSurface3)};
  RepresentativeSurfaceSet expected_surface_set(expected_surface_list.begin(),
                                                expected_surface_list.end());
  EXPECT_EQ(expected_surface_set, equivalence.GetRepresentatives(surface_set));
  EXPECT_EQ(expected_surface_list,
            equivalence.GetRepresentatives(surface_list));
}

TEST(SurfaceSetEquivalenceTest, OverlappingEquivalenceSets) {
  // kSurface1 and kSurface2 are in an equivalence set. kSurface1 is the
  // representative.
  base::FieldTrialParams params = {
      {features::kIdentifiabilityStudySurfaceEquivalenceClasses.name,
       "100;257,257;301,300;301"}};
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kIdentifiabilityStudy,
                                              params);
  SurfaceSetEquivalence equivalence;

  EXPECT_EQ(kSurface1, equivalence.GetRepresentative(kSurface2).value());
  EXPECT_EQ(kSurface3, equivalence.GetRepresentative(kSurface4).value());
}
