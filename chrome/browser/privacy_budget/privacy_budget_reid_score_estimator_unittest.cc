// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_reid_score_estimator.h"

#include <cstdint>

#include "base/containers/flat_map.h"
#include "chrome/browser/privacy_budget/identifiability_study_group_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"

TEST(PrivacyBudgetReidScoreEstimatorStandaloneTest, EmptyStorageByDefault) {
  auto settings =
      IdentifiabilityStudyGroupSettings::InitFrom(true, 0, 0, "", "", "", "");
  auto reid_storage =
      std::make_unique<PrivacyBudgetReidScoreEstimator>(settings);

  // By default the Reid storage should be empty if nothing is provided in
  // settings:
  EXPECT_TRUE(reid_storage->GetSurfacesAndValuesForTesting().empty());

  // If no list set in settings, ProcessForReidScore will do nothing.
  const auto surface = blink::IdentifiableSurface::FromMetricHash(1);
  const blink::IdentifiableToken token = 1;
  reid_storage->ProcessForReidScore(surface, token);
  EXPECT_TRUE(reid_storage->GetSurfacesAndValuesForTesting().empty());
}

TEST(PrivacyBudgetReidScoreEstimatorStandaloneTest,
     InitializeStorageWithEmptyMaps) {
  // Initialize the settings with random surface keys.
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      true, 0, 0, "", "", "",
      "2077075229;1122849309,3996426525;517825053;1983694109");
  auto reid_storage =
      std::make_unique<PrivacyBudgetReidScoreEstimator>(settings);

  auto* surface_maps = &reid_storage->GetSurfacesAndValuesForTesting();

  // Storage should contain 2 entries.
  EXPECT_TRUE(surface_maps->size() == 2);

  // Create manually one of the expected Reid surface keys.
  constexpr auto kReidScoreType =
      blink::IdentifiableSurface::Type::kReidScoreEstimator;
  auto surface_1 = blink::IdentifiableSurface::FromMetricHash(2077075229);
  auto surface_2 = blink::IdentifiableSurface::FromMetricHash(1122849309);
  std::vector<blink::IdentifiableToken> tokens{surface_1.GetInputHash(),
                                               surface_2.GetInputHash()};
  auto expected_surface = blink::IdentifiableSurface::FromTypeAndToken(
      kReidScoreType, base::make_span(tokens));

  // The storage should have the expected key.
  EXPECT_TRUE(surface_maps->contains(expected_surface));

  // Check if expected Reid surface has the map of its surfaces.
  auto submap_itr = surface_maps->find(expected_surface);
  EXPECT_TRUE(submap_itr != surface_maps->end());
  EXPECT_TRUE(submap_itr->second.size() == 2);
  EXPECT_TRUE(submap_itr->second.contains(surface_1));
  EXPECT_TRUE(submap_itr->second.contains(surface_2));
}

TEST(PrivacyBudgetReidScoreEstimatorStandaloneTest,
     UpdateStorageWithNewValues) {
  // Initialize the settings with random surface keys.
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      true, 0, 0, "", "", "", "2077075229;1122849309");
  auto reid_storage =
      std::make_unique<PrivacyBudgetReidScoreEstimator>(settings);

  auto* surface_maps = &reid_storage->GetSurfacesAndValuesForTesting();

  // Making sure the storage is not empty.
  EXPECT_TRUE(surface_maps->size() == 1);

  // Process values for 2 surfaces.
  auto surface_1 = blink::IdentifiableSurface::FromMetricHash(2077075229);
  auto surface_2 = blink::IdentifiableSurface::FromMetricHash(1122849309);
  int64_t token1 = 1234;
  int64_t token2 = 1235;

  reid_storage->ProcessForReidScore(surface_1,
                                    blink::IdentifiableToken(token1));
  reid_storage->ProcessForReidScore(surface_2,
                                    blink::IdentifiableToken(token2));

  auto map_itr = surface_maps->begin();
  auto submap_itr = map_itr->second;

  // The respective surface should have a value equal to the respective
  auto surface_itr = submap_itr.find(surface_1);
  EXPECT_TRUE(surface_itr->second.has_value());
  EXPECT_TRUE(surface_itr->second->ToUkmMetricValue() == token1);

  surface_itr = submap_itr.find(surface_2);

  EXPECT_TRUE(surface_itr->second.has_value());
  EXPECT_TRUE(surface_itr->second->ToUkmMetricValue() == token2);
}
