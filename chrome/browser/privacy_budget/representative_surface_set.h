// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_REPRESENTATIVE_SURFACE_SET_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_REPRESENTATIVE_SURFACE_SET_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/strong_alias.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace privacy_budget_internal {
template <typename T>
struct SortWhenSerializing;
}  // namespace privacy_budget_internal

// Typesafe version of blink::IdentifiableSurface that indicates that the
// surface is also a representative for its equivalence class. See
// SurfaceSetEquivalence for details.
//
// Without something like this, we'll have to manually keep track of which
// surfaces are generic and which are representatives.
using RepresentativeSurface = base::StrongAlias<class RepresentativeSurfaceTag,
                                                blink::IdentifiableSurface>;

// Collections corresponding to blink::IdentifiableSurface collections.
using RepresentativeSurfaceSet = base::flat_set<RepresentativeSurface>;
using RepresentativeSurfaceList = std::vector<RepresentativeSurface>;

// Converts a set of typesafe representatives surface identifiers to a set of
// regular 'ol `IdentifiableSurface`.
//
// To get a collection of RepresentativeSurface values from a collection of
// blink::IdentifiableSurface values, use SurfaceSetEquivalence.
IdentifiableSurfaceSet IdentifiableSurfaceSetFromRepresentative(
    const RepresentativeSurfaceSet& source);

// The returned string will be a decimal representation of the numeric value of
// the underlying surface.
std::string RepresentativeSurfaceToString(const RepresentativeSurface& v);

// For unordered containers.
struct RepresentativeSurfaceHash {
  size_t operator()(const RepresentativeSurface& s) const;
};

namespace privacy_budget_internal {
template <>
struct SortWhenSerializing<RepresentativeSurfaceSet> : std::true_type {};
}  // namespace privacy_budget_internal

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_REPRESENTATIVE_SURFACE_SET_H_
