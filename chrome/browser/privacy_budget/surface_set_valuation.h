// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_SURFACE_SET_VALUATION_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_SURFACE_SET_VALUATION_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/privacy_budget/representative_surface_set.h"
#include "chrome/browser/privacy_budget/surface_set_equivalence.h"
#include "chrome/common/privacy_budget/privacy_budget_settings_provider.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

// Estimates the privacy budget cost for a set of identifiable surfaces.
//
// Random surface selection is guided by a rudimentary naïve costing model.
// _This model is in no way meant to resemble any model that will result from
// the identifiability study_. It currently has the following properties:
//
// * By default all surfaces are assumed to have a cost of 1 median surface.
//
//     * Units are in information exposed by a _median _surface. So sans
//       anything more specific the costing model assumes that a given surface
//       exposes a median amount of identifiable information _independently of
//       any other surface_.
//
//       This choice of unit has the desirable property of being compatible with
//       just counting the number of surfaces.
//
//     * The measure is proportional to the exposed Shannon entropy. I.e. if
//       a single unit corresponds to 𝛮 bits, then 2 units will correspond to 2𝛮
//       bits. Hence the information content of two independent identifiable
//       surfaces is the sum of the information content of each individual
//       surface.
//
// * The cost of any individual surface can be overridden on the basis of its
//   type or its identifier. This is for dealing with substantial deviations
//   from the median.
//
// * A group of surfaces can be treated as a single unit and be assigned a group
//   cost. This is useful, for example, if we want to select `Screen.width` and
//   `Screen.height` together so that any time we decide to include one we
//   implicitly include the other.
//
//   The set of surfaces that are considered as a unit for valuation purposes is
//   referred to in code as an "equivalence class" or "equivalence set."
//
// * Given a set of surfaces 𝑺 the cost of the set is the sum of:
//
//   * The costs of each surface that is _not_ a member of any equivalence
//     class.
//
//   * The cost of each equivalence class where at least one of the surfaces in
//     𝑺 is a member.
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
  const SurfaceSetEquivalence& equivalence() const {
    return *equivalence_sets_;
  }

  // Returns the expected number of surfaces that would fit in `cost`. This is
  // a naive estimate that assumes that the surfaces will be of average cost and
  // mutually independent.
  //
  // `cost` must be positive.
  static size_t ExpectedSurfaceCountForCost(PrivacyBudgetCost cost);

 private:
  const raw_ref<const SurfaceSetEquivalence> equivalence_sets_;

  // Per surface relative cost.
  const IdentifiableSurfaceCostMap per_surface_costs_;

  // Per surface type relative cost.
  const IdentifiableSurfaceTypeCostMap per_type_costs_;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_SURFACE_SET_VALUATION_H_
