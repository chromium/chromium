// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_reid_score_estimator.h"

#include "base/containers/flat_map.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"

PrivacyBudgetReidScoreEstimator::PrivacyBudgetReidScoreEstimator(
    const IdentifiabilityStudyGroupSettings& state_settings) {
  // Initialize the Reid map with the surfaces from study settings:
  // Step 1: Get the list of blocks of surfaces from state settings.
  IdentifiableSurfaceBlocks reid_blocks = state_settings.reid_blocks();
  // Step 2: Get the type of the Reid surface.
  constexpr auto kReidScoreType =
      blink::IdentifiableSurface::Type::kReidScoreEstimator;

  // Step 3: Create an IdentifiableSurface key from every block and add to
  // storage.
  for (const IdentifiableSurfaceList& surface_list : reid_blocks) {
    // For every list of surfaces, create a map and add it to storage.
    SurfacesAndOptionalValues surface_map;

    // Used to construct the Reid Surface Key.
    std::vector<blink::IdentifiableToken> tokens;

    // Add the list of surfaces to the map and add the surface value to token
    // vector to prepare for the Reid key generation.
    for (const blink::IdentifiableSurface& surface : surface_list) {
      surface_map.insert_or_assign(surface,
                                   absl::optional<blink::IdentifiableToken>());
      tokens.emplace_back(surface.GetInputHash());
    }
    // Create surface key of Reid score type corresponding to this block.
    blink::IdentifiableSurface reid_key =
        blink::IdentifiableSurface::FromTypeAndToken(kReidScoreType,
                                                     base::make_span(tokens));
    // Add the map surfaces to the storage under the new Reid key.
    surfaces_and_values_.insert_or_assign(reid_key, surface_map);
  }
}

PrivacyBudgetReidScoreEstimator::~PrivacyBudgetReidScoreEstimator() = default;

const base::flat_map<blink::IdentifiableSurface, SurfacesAndOptionalValues>&
PrivacyBudgetReidScoreEstimator::GetSurfacesAndValuesForTesting() {
  return surfaces_and_values_;
}

void PrivacyBudgetReidScoreEstimator::ProcessForReidScore(
    blink::IdentifiableSurface surface,
    blink::IdentifiableToken token) {
  // Assign the token value to the surface if found in the list of surface maps.
  for (auto& map_itr : surfaces_and_values_) {
    SurfacesAndOptionalValues* surface_map = &map_itr.second;
    auto surface_itr = surface_map->find(surface);
    if (surface_itr != surface_map->end()) {
      surface_itr->second = token;
    }
  }
}
