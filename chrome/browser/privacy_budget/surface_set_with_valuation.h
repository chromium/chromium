// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_SURFACE_SET_WITH_VALUATION_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_SURFACE_SET_WITH_VALUATION_H_

#include "base/containers/flat_tree.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/privacy_budget/representative_surface_set.h"
#include "chrome/browser/privacy_budget/surface_set_equivalence.h"
#include "chrome/browser/privacy_budget/surface_set_valuation.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

// A set-like container for blink::IdentifiableSurface and RepresentativeSurface
// that maintains an aggregate surface cost (via SurfaceSetValuation).
//
// * The container only consumes representative surfaces (see
//   RepresentativeSurface).
//
// * Adding a RepresentativeSurface is the equivalent of adding all the
//   surfaces that are in the same equivalence class. I.e. contains(s) will
//   return true for all `s` that are in the same equivalence class as the
//   representative that was added.
//
// * Adding a IdentifiableSurface is the equivalent of adding its corresponding
//   RepresentativeSurface, and thus the equivalent of adding all the surfaces
//   in its equivalence class.
class SurfaceSetWithValuation {
 public:
  explicit SurfaceSetWithValuation(const SurfaceSetValuation& valuation);
  ~SurfaceSetWithValuation();

  using container_type = RepresentativeSurfaceSet;
  using key_type = container_type::key_type;
  using value_type = container_type::value_type;
  using const_iterator = container_type::const_iterator;
  using size_type = container_type::size_type;

  const_iterator begin() const noexcept { return surfaces_.begin(); }
  const_iterator end() const noexcept { return surfaces_.end(); }

  bool Empty() const { return surfaces_.empty(); }
  size_type Size() const { return surfaces_.size(); }

  // Returns the current cost of the surfaces included in the container. This
  // function is very cheap to use.
  PrivacyBudgetCost Cost() const { return cost_; }

  // All mutations to the underlying data must be done through one of these
  // functions. All others are intentionally written to not return a non-const
  // reference nor cause mutations of the underlying container.

  // Try to add surface `surface` while keeping the cost under `budget`.
  // Returns true if successful. No changes are made if the surface cannot be
  // added without exceeding the budget.
  //
  // Adding a surface that's already in the container -- either explicitly or
  // due to its equivalence class being a member of the container -- results in
  // a return value of `true`.
  bool TryAdd(RepresentativeSurface surface, PrivacyBudgetCost budget);

  // Try to add surface `surface` while keeping the cost under `budget`.
  // Returns true if successful. No changes are made if the surface cannot be
  // added without exceeding the budget.
  //
  // Adding a surface that's already in the container -- either explicitly or
  // due to its equivalence class being a member of the container -- results in
  // a return value of `true`.
  bool TryAdd(blink::IdentifiableSurface surface, PrivacyBudgetCost budget);

  // Assign a set of surfaces. Any existing surfaces in this container will be
  // removed.
  //
  // If the surfaces in `container` exceed the cost set out in `budget`, then
  // this function removes random elements in `container` until the cost falls
  // below or meets `budget`.
  void AssignWithBudget(RepresentativeSurfaceSet&& container,
                        PrivacyBudgetCost budget);

  // Assign a set of surfaces. Any existing surfaces in this container will be
  // removed.
  void Assign(RepresentativeSurfaceSet&& container);

  // Removes all surfaces from this container.
  void Clear();

  // Returns a reference to the underlying container.
  const RepresentativeSurfaceSet& Container() const { return surfaces_; }

  // Acquire the underlying container.
  RepresentativeSurfaceSet Take() &&;

  bool contains(RepresentativeSurface k) const {
    return surfaces_.find(k) != surfaces_.end();
  }
  bool contains(const blink::IdentifiableSurface surface) const {
    return contains(valuation_->equivalence().GetRepresentative(surface));
  }

 private:
  const raw_ref<const SurfaceSetValuation> valuation_;
  RepresentativeSurfaceSet surfaces_;
  PrivacyBudgetCost cost_ = 0.0;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_SURFACE_SET_WITH_VALUATION_H_
