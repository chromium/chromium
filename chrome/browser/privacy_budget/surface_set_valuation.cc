// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/surface_set_valuation.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <tuple>
#include <type_traits>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "chrome/browser/privacy_budget/representative_surface_set.h"
#include "chrome/browser/privacy_budget/surface_set_equivalence.h"
#include "chrome/common/privacy_budget/field_trial_param_conversions.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {

IdentifiableSurfaceCostMap GetPerSurfaceCostsFromFieldTrial() {
  return DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceCostMap>(
      features::kIdentifiabilityStudyPerHashCost.Get());
}

IdentifiableSurfaceTypeCostMap GetPerTypeCostsFromFieldTrial() {
  return DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceTypeCostMap>(
      features::kIdentifiabilityStudyPerTypeCost.Get());
}

}  // namespace

SurfaceSetValuation::SurfaceSetValuation(
    const SurfaceSetEquivalence& surface_set_equivalence)
    : equivalence_sets_(surface_set_equivalence),
      per_surface_costs_(GetPerSurfaceCostsFromFieldTrial()),
      per_type_costs_(GetPerTypeCostsFromFieldTrial()) {}

SurfaceSetValuation::~SurfaceSetValuation() = default;

// static
const double SurfaceSetValuation::kDefaultCost;

double SurfaceSetValuation::Cost(const IdentifiableSurfaceSet& set) const {
  return Cost(equivalence_sets_->GetRepresentatives(set));
}

double SurfaceSetValuation::Cost(const RepresentativeSurfaceSet& set) const {
  double cost = 0.0;
  // This is a naive calculation that assumes that surfaces within an
  // equivalence class is perfectly correlated with each other while surfaces
  // that are not in the same class are perfectly independent. In reality
  // nothing is quite cut and dry. Until we have a better model we are going to
  // use this one.
  for (auto surface : set)
    cost += Cost(surface);
  return cost;
}

double SurfaceSetValuation::Cost(blink::IdentifiableSurface surface) const {
  return Cost(equivalence_sets_->GetRepresentative(surface));
}

double SurfaceSetValuation::Cost(RepresentativeSurface surface) const {
  auto surface_cost_iter = per_surface_costs_.find(surface.value());
  if (surface_cost_iter != per_surface_costs_.end())
    return surface_cost_iter->second;

  auto type_cost_iter = per_type_costs_.find(surface->GetType());
  if (type_cost_iter != per_type_costs_.end())
    return type_cost_iter->second;

  return kDefaultCost;
}

double SurfaceSetValuation::IncrementalCost(
    const RepresentativeSurfaceSet& prior,
    RepresentativeSurface new_addition) const {
  if (base::Contains(prior, new_addition))
    return 0.0;
  return Cost(new_addition);
}

size_t SurfaceSetValuation::ExpectedSurfaceCountForCost(double cost) {
  // The units are already in expected surface counts.
  return base::ClampCeil<size_t>(cost);
}
