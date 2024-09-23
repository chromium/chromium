// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_SURFACE_SET_EQUIVALENCE_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_SURFACE_SET_EQUIVALENCE_H_

#include <string_view>

#include "base/containers/flat_map.h"
#include "chrome/browser/privacy_budget/representative_surface_set.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

// Evaluates surface set equivalence.
//
// A surface set equivalence class is a set of identifiable surfaces that have
// a high degree of pairwise correlation within the set. In other words
// exposure of any surface in the set is equivalent to the exposure of any and
// all other surfaces in the set.
//
// Much like a Union-Find data structure, every equivalence class has
// a representative surface. The cost of any and all surfaces in the set is the
// cost of the representative surface.
class SurfaceSetEquivalence {
 public:
  SurfaceSetEquivalence();
  ~SurfaceSetEquivalence();

  // Determines the representative surface for the equivalence class containing
  // `surface`. Returns `surface` unchanged if it is not a member of any
  // equivalence class. Either way all surface inclusion and costing decisions
  // should be made based on the returned surface.
  RepresentativeSurface GetRepresentative(
      blink::IdentifiableSurface surface) const;

  RepresentativeSurfaceSet GetRepresentatives(
      const IdentifiableSurfaceSet& source) const;

  RepresentativeSurfaceList GetRepresentatives(
      const IdentifiableSurfaceList& source) const;

  bool IsRepresentative(blink::IdentifiableSurface surface) const;

  bool IsRepresentative(const IdentifiableSurfaceSet& source) const;

 private:
  using EquivalenceClassIdentifierMap =
      base::flat_map<blink::IdentifiableSurface, RepresentativeSurface>;

  static EquivalenceClassIdentifierMap DecodeEquivalenceClassSet(
      std::string_view encoded_class_set);

  // Maps an IdentifiableSurface to its corresponding representative surface.
  //
  // Invariants:
  //
  //   * (s,s) ∉ equivalence_map_.
  //
  //   * (s,∙) ∈ equivalence_map_ ⇒  (∙,s) ∉ equivalence_map_.
  //
  //   * (∙,s) ∈ equivalence_map_ ⇒  (s,∙) ∉ equivalence_map_.
  //
  const EquivalenceClassIdentifierMap equivalence_map_;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_SURFACE_SET_EQUIVALENCE_H_
