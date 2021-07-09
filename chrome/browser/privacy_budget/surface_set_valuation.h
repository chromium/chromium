// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_SURFACE_SET_VALUATION_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_SURFACE_SET_VALUATION_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/privacy_budget/representative_surface_set.h"
#include "chrome/browser/privacy_budget/surface_set_equivalence.h"
#include "chrome/common/privacy_budget/privacy_budget_settings_provider.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

// Estimates the privacy budget cost for a set of identifiable surfaces.
//
// The costing model is currently naive. The cost is reported based on an
// arbitrary unit whose value is 1.0 for a "median" identifiable surface. Costs
// for other surfaces are measured relative to the median surface. Scale should
// be assumed to be roughly logarithmic. See documentation for
// `PrivacyBudgetCost` for more details.
//
// All costs returned by the costing functions should be considered to be
// _upper bounds_ on the actual cost. I.e. these functions will always
// over-estimate.
class SurfaceSetValuation {
 public:
  // The default cost is always one median identifiable surface.
  static constexpr double kDefaultCost = 1.0;

  // Constructs a valuation object. The SurfaceSetEquivalence object passed in
  // by reference as `equivalence` MUST outlive this object.
  explicit SurfaceSetValuation(const SurfaceSetEquivalence& equivalence);
  ~SurfaceSetValuation();

  // Returns an upper-bound for the cost of the surfaces in `set`.
  PrivacyBudgetCost Cost(const IdentifiableSurfaceSet& set) const;

  // Returns an upper-bound for the cost of `surface`.
  PrivacyBudgetCost Cost(blink::IdentifiableSurface surface) const;

  // Returns an upper-bound for the cost of `surface`.
  PrivacyBudgetCost Cost(RepresentativeSurface surface) const;

  // Returns an upper-bound for the cost of the surfaces in `set`.
  PrivacyBudgetCost Cost(const RepresentativeSurfaceSet& set) const;

  // Returns the _incremental_ change in cost that would result from adding
  // `new_addition` to the set of surfaces represented by `prior`.
  //
  // Costs are always zero or positive, so the returned value will never be
  // negative.
  PrivacyBudgetCost IncrementalCost(const RepresentativeSurfaceSet& prior,
                                    RepresentativeSurface new_addition) const;

  // Returns a reference to the underlying identifiable surface equivalence
  // model.
  const SurfaceSetEquivalence& equivalence() const { return equivalence_sets_; }

 private:
  const SurfaceSetEquivalence& equivalence_sets_;

  // Per surface relative cost.
  const IdentifiableSurfaceCostMap per_surface_costs_;

  // Per surface type relative cost.
  const IdentifiableSurfaceTypeCostMap per_type_costs_;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_SURFACE_SET_VALUATION_H_
