// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/identifiability_study_group_settings.h"

#include <numeric>

#include "base/containers/flat_map.h"
#include "base/cxx17_backports.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {

void UmaHistogramFinchConfigValidation(bool valid) {
  base::UmaHistogramBoolean(
      "PrivacyBudget.Identifiability.FinchConfigValidationResult", valid);
}

}  // namespace

// static
IdentifiabilityStudyGroupSettings
IdentifiabilityStudyGroupSettings::InitFromFeatureParams() {
  return InitFrom(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy),
                  features::kIdentifiabilityStudyExpectedSurfaceCount.Get(),
                  features::kIdentifiabilityStudyActiveSurfaceBudget.Get(),
                  features::kIdentifiabilityStudyBlocks.Get(),
                  features::kIdentifiabilityStudyBlockWeights.Get(),
                  features::kIdentifiabilityStudyAllowedRandomTypes.Get(),
                  features::kIdentifiabilityStudyReidSurfaceBlocks.Get());
}

// static
IdentifiabilityStudyGroupSettings IdentifiabilityStudyGroupSettings::InitFrom(
    bool enabled,
    int expected_surface_count,
    int surface_budget,
    const std::string& blocks,
    const std::string& blocks_weights,
    const std::string& allowed_random_types,
    const std::string& reid_blocks) {
  return IdentifiabilityStudyGroupSettings(
      enabled, !blocks.empty(), !reid_blocks.empty(), expected_surface_count,
      surface_budget,
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceBlocks>(blocks),
      DecodeIdentifiabilityFieldTrialParam<std::vector<double>>(blocks_weights),
      DecodeIdentifiabilityFieldTrialParam<
          std::vector<blink::IdentifiableSurface::Type>>(allowed_random_types),
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceBlocks>(
          reid_blocks));
}

IdentifiabilityStudyGroupSettings::IdentifiabilityStudyGroupSettings(
    bool enabled,
    bool is_using_assigned_block_sampling,
    bool is_using_reid_score_estimator,
    int expected_surface_count,
    int surface_budget,
    IdentifiableSurfaceBlocks blocks,
    std::vector<double> blocks_weights,
    std::vector<blink::IdentifiableSurface::Type> allowed_random_types,
    IdentifiableSurfaceBlocks reid_blocks)
    : enabled_(enabled),
      is_using_assigned_block_sampling_(is_using_assigned_block_sampling),
      is_using_reid_score_estimator_(is_using_reid_score_estimator),
      expected_surface_count_(base::clamp<int>(
          expected_surface_count,
          0,
          features::kMaxIdentifiabilityStudyExpectedSurfaceCount)),
      surface_budget_(base::clamp<int>(
          surface_budget,
          0,
          features::kMaxIdentifiabilityStudyActiveSurfaceBudget)),
      blocks_(std::move(blocks)),
      blocks_weights_(std::move(blocks_weights)),
      reid_blocks_(std::move(reid_blocks)),
      allowed_random_types_(std::move(allowed_random_types)) {
  bool validates = Validate();
  UmaHistogramFinchConfigValidation(validates);
  if (!validates)
    enabled_ = false;
}

IdentifiabilityStudyGroupSettings::~IdentifiabilityStudyGroupSettings() =
    default;
IdentifiabilityStudyGroupSettings::IdentifiabilityStudyGroupSettings(
    IdentifiabilityStudyGroupSettings&&) = default;

bool IdentifiabilityStudyGroupSettings::Validate() {
  // Disabling the Identifiability Study feature flag is a valid configuration.
  if (!enabled_)
    return true;
  // If the study is enabled, at least one of assigned-block-sampling or
  // reid-score-estimation or random-surface-assignment should be enabled.
  if (!is_using_assigned_block_sampling_ && !is_using_reid_score_estimator_ &&
      expected_surface_count_ == 0) {
    return false;
  }
  if (is_using_assigned_block_sampling_ && !ValidateAssignedBlockSampling())
    return false;
  if (is_using_reid_score_estimator_ && !ValidateReidBlockEstimator())
    return false;

  return true;
}

bool IdentifiabilityStudyGroupSettings::ValidateAssignedBlockSampling() {
  // For every block there should be a weight.
  if (blocks_weights_.size() != blocks_.size())
    return false;

  // Weights should be positive.
  for (double weight : blocks_weights_) {
    if (weight < 0)
      return false;
  }

  // Each single surface should have probability lower than the threshold to be
  // selected.
  base::flat_map<blink::IdentifiableSurface, double> weight_per_surface;
  for (size_t i = 0; i < blocks_.size(); ++i) {
    for (const blink::IdentifiableSurface& surface : blocks_[i]) {
      auto el = weight_per_surface.find(surface);
      if (el != weight_per_surface.end()) {
        weight_per_surface.insert_or_assign(surface,
                                            el->second + blocks_weights_[i]);
      } else {
        weight_per_surface.insert_or_assign(surface, blocks_weights_[i]);
      }
    }
  }
  double sum_weights =
      std::accumulate(blocks_weights_.begin(), blocks_weights_.end(), 0.0);
  for (const auto& iter : weight_per_surface) {
    double surface_probability = iter.second / sum_weights;
    if (surface_probability > features::kMaxProbabilityPerSurface)
      return false;
  }

  return true;
}

bool IdentifiabilityStudyGroupSettings::ValidateReidBlockEstimator() {
  // Currently return true if the Reid blocks is not empty.
  // This validation will change once we introduce the offset value for every
  // Reid block list.
  if (!reid_blocks_.empty()) {
    return true;
  }
  return false;
}

const IdentifiableSurfaceBlocks& IdentifiabilityStudyGroupSettings::blocks()
    const {
  DCHECK(is_using_assigned_block_sampling_);
  return blocks_;
}

const std::vector<double>& IdentifiabilityStudyGroupSettings::blocks_weights()
    const {
  DCHECK(is_using_assigned_block_sampling_);
  return blocks_weights_;
}

const IdentifiableSurfaceBlocks&
IdentifiabilityStudyGroupSettings::reid_blocks() const {
  return reid_blocks_;
}

const std::vector<blink::IdentifiableSurface::Type>&
IdentifiabilityStudyGroupSettings::allowed_random_types() const {
  DCHECK(!is_using_assigned_block_sampling_);
  return allowed_random_types_;
}
