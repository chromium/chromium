// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/surface_set_equivalence.h"

#include <iterator>
#include <set>
#include <string_view>

#include "base/containers/contains.h"
#include "chrome/common/privacy_budget/field_trial_param_conversions.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"

SurfaceSetEquivalence::EquivalenceClassIdentifierMap
SurfaceSetEquivalence::DecodeEquivalenceClassSet(std::string_view param_value) {
  EquivalenceClassIdentifierMap representative_map;
  auto surface_set_set =
      DecodeIdentifiabilityFieldTrialParam<SurfaceSetEquivalentClassesList>(
          param_value);

  // A surface must only appear once as a key _or_ as a value in our mapping.
  // Otherwise the mapping will violate the invariants.
  std::set<blink::IdentifiableSurface> seen;

  for (const auto& surface_set : surface_set_set) {
    // A single surface set or an empty one is a no-op. These shouldn't be sent
    // as a part of a valid configuration, but the code shouldn't get confused
    // upon seeing one either.
    if (surface_set.size() <= 1)
      continue;

    auto first_surface = surface_set.front();

    if (base::Contains(seen, first_surface))
      continue;
    seen.insert(first_surface);

    auto representative = RepresentativeSurface(first_surface);

    for (const auto& surface : surface_set) {
      if (!base::Contains(seen, surface)) {
        seen.insert(surface);
        representative_map[surface] = representative;
      }
    }
  }

  return representative_map;
}

SurfaceSetEquivalence::SurfaceSetEquivalence()
    : equivalence_map_(DecodeEquivalenceClassSet(
          features::kIdentifiabilityStudySurfaceEquivalenceClasses.Get())) {}

SurfaceSetEquivalence::~SurfaceSetEquivalence() = default;

RepresentativeSurface SurfaceSetEquivalence::GetRepresentative(
    blink::IdentifiableSurface surface) const {
  auto it = equivalence_map_.find(surface);
  if (it == equivalence_map_.end())
    return RepresentativeSurface(surface);
  return RepresentativeSurface(it->second);
}

RepresentativeSurfaceSet SurfaceSetEquivalence::GetRepresentatives(
    const IdentifiableSurfaceSet& source) const {
  return base::MakeFlatSet<RepresentativeSurface>(
      source, {},
      [this](const auto& surface) { return GetRepresentative(surface); });
}

RepresentativeSurfaceList SurfaceSetEquivalence::GetRepresentatives(
    const IdentifiableSurfaceList& source) const {
  RepresentativeSurfaceList result;
  result.reserve(source.size());
  std::set<RepresentativeSurface> seen;
  for (const auto surface : source) {
    auto representative = GetRepresentative(surface);
    auto inserted = seen.insert(representative);
    if (inserted.second)
      result.push_back(representative);
  }
  result.shrink_to_fit();
  return result;
}

bool SurfaceSetEquivalence::IsRepresentative(
    blink::IdentifiableSurface surface) const {
  return surface == GetRepresentative(surface).value();
}

bool SurfaceSetEquivalence::IsRepresentative(
    const IdentifiableSurfaceSet& source) const {
  return base::ranges::all_of(source,
                              [this](auto s) { return IsRepresentative(s); });
}
