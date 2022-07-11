// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_reid_score_estimator.h"

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/rand_util.h"
#include "chrome/common/privacy_budget/types.h"
#include "components/ukm/ukm_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"

namespace {
void ReportHashForReidScore(blink::IdentifiableSurface surface,
                            uint64_t reid_hash) {
  ukm::SourceId ukm_source_id = ukm::NoURLSourceId();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  blink::IdentifiabilityMetricBuilder(ukm_source_id)
      .Add(surface, reid_hash)
      .Record(ukm_recorder);
  blink::IdentifiabilitySampleCollector::Get()->FlushSource(ukm_recorder,
                                                            ukm_source_id);
}
}  // namespace

PrivacyBudgetReidScoreEstimator::PrivacyBudgetReidScoreEstimator(
    const IdentifiabilityStudyGroupSettings& state_settings) {
  // Initialize the Reid map with the surfaces from study settings:
  // Step 1: Get the list of blocks of surfaces from state settings.
  IdentifiableSurfaceBlocks reid_blocks = state_settings.reid_blocks();
  reid_blocks_salts_ranges_ = state_settings.reid_blocks_salts_ranges();

  // Step 2: Get the type of the Reid surface.
  constexpr auto kReidScoreType =
      blink::IdentifiableSurface::Type::kReidScoreEstimator;

  // Step 3: Create an IdentifiableSurface key from every block and add to
  // storage.
  for (const IdentifiableSurfaceList& surface_list : reid_blocks) {
    // For every list of surfaces, create a map and add it to storage.
    SurfacesAndOptionalValues surface_map;
    count_flag_.push_back(0);

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
  int i = 0;
  for (auto& map_itr : surfaces_and_values_) {
    SurfacesAndOptionalValues* surface_map = &map_itr.second;
    // Skip if the surface map is full and has all its values.
    if (count_flag_[i] < surface_map->size()) {
      auto surface_itr = surface_map->find(surface);
      if (surface_itr != surface_map->end()) {
        if (!surface_itr->second.has_value()) {
          ++count_flag_[i];
        }
        surface_itr->second = token;
        // Report new Reid surface if the map is full.
        if (count_flag_[i] == surface_map->size()) {
          // Compute the Reid hash for the needed Reid block.
          uint64_t reid_hash = ComputeHashForReidScore(
              *surface_map, reid_blocks_salts_ranges_.at(i));
          // Report to UKM in a separate task in order to avoid re-entrancy.
          base::SequencedTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, base::BindOnce(&ReportHashForReidScore, map_itr.first,
                                        reid_hash));
        }
      }
    }
    ++i;
  }
}

uint64_t PrivacyBudgetReidScoreEstimator::ComputeHashForReidScore(
    const SurfacesAndOptionalValues& surface_map,
    uint64_t max_num_salt) {
  std::vector<uint64_t> tokens;
  uint64_t salt = base::RandGenerator(max_num_salt);

  tokens.emplace_back(salt);
  for (auto& surface_itr : surface_map) {
    tokens.emplace_back(
        static_cast<uint64_t>(surface_itr.second->ToUkmMetricValue()));
  }
  // Use the hash function embedded in IdentifiableToken.
  uint64_t reid_hash = blink::IdentifiabilityDigestOfBytes(
      base::as_bytes(base::make_span(tokens)));
  // Return salt in left 32 bits and Reid 1-bit hash in right 32 bits.
  return ((salt << 32) | (reid_hash % 2));
}
