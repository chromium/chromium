// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/identifiability_study_group_settings.h"

#include <algorithm>
#include <numeric>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
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
                  features::kIdentifiabilityStudyAllowedRandomTypes.Get());
}

// static
IdentifiabilityStudyGroupSettings IdentifiabilityStudyGroupSettings::InitFrom(
    bool enabled,
    int expected_surface_count,
    int surface_budget,
    const std::string& blocks,
    const std::string& blocks_weights,
    const std::string& allowed_random_types) {
  return IdentifiabilityStudyGroupSettings(
      enabled, expected_surface_count, surface_budget,
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceBlocks>(blocks),
      DecodeIdentifiabilityFieldTrialParam<std::vector<double>>(blocks_weights),
      DecodeIdentifiabilityFieldTrialParam<
          std::vector<blink::IdentifiableSurface::Type>>(allowed_random_types));
}

IdentifiabilityStudyGroupSettings::IdentifiabilityStudyGroupSettings(
    bool enabled,
    int expected_surface_count,
    int surface_budget,
    IdentifiableSurfaceBlocks blocks,
    std::vector<double> blocks_weights,
    std::vector<blink::IdentifiableSurface::Type> allowed_random_types)
    : enabled_(enabled),
      expected_surface_count_(std::clamp<int>(
          expected_surface_count,
          0,
          features::kMaxIdentifiabilityStudyExpectedSurfaceCount)),
      surface_budget_(std::clamp<int>(
          surface_budget,
          0,
          features::kMaxIdentifiabilityStudyActiveSurfaceBudget)),
      blocks_(std::move(blocks)),
      blocks_weights_(std::move(blocks_weights)),
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
  // random-surface-assignment should be enabled.
  if (!IsUsingAssignedBlockSampling() && !IsUsingRandomSampling()) {
    return false;
  }
  if (IsUsingAssignedBlockSampling() && IsUsingRandomSampling())
    return false;
  if (IsUsingAssignedBlockSampling() && !ValidateAssignedBlockSampling())
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

const IdentifiableSurfaceBlocks& IdentifiabilityStudyGroupSettings::blocks()
    const {
  return blocks_;
}

const std::vector<double>& IdentifiabilityStudyGroupSettings::blocks_weights()
    const {
  return blocks_weights_;
}

const std::vector<blink::IdentifiableSurface::Type>&
IdentifiabilityStudyGroupSettings::allowed_random_types() const {
  return allowed_random_types_;
}

bool IdentifiabilityStudyGroupSettings::IsUsingAssignedBlockSampling() const {
  return !blocks().empty();
}

bool IdentifiabilityStudyGroupSettings::IsUsingRandomSampling() const {
  return expected_surface_count() > 0;
}

bool IdentifiabilityStudyGroupSettings::IsUsingSamplingOfSurfaces() const {
  // Random and assigned block sampling are mutually exclusive.
  DCHECK(!IsUsingRandomSampling() || !IsUsingAssignedBlockSampling());
  return IsUsingRandomSampling() || IsUsingAssignedBlockSampling();
}
