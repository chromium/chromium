// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/surface_set_with_valuation.h"

#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"

static_assert(std::is_same<RepresentativeSurface,
                           SurfaceSetWithValuation::key_type>::value,
              "");

SurfaceSetWithValuation::SurfaceSetWithValuation(
    const SurfaceSetValuation& valuation)
    : valuation_(valuation) {}

SurfaceSetWithValuation::~SurfaceSetWithValuation() = default;

bool SurfaceSetWithValuation::TryAdd(RepresentativeSurface surface,
                                     PrivacyBudgetCost budget) {
  if (cost_ > budget)
    return false;

  auto it = surfaces_.find(surface);
  if (it != surfaces_.end())
    return true;

  double new_cost = cost_ + valuation_->IncrementalCost(surfaces_, surface);
  if (new_cost > budget)
    return false;

  cost_ = new_cost;
  auto insertion_result = surfaces_.insert(surface);
  DCHECK(insertion_result.second);
  return true;
}

bool SurfaceSetWithValuation::TryAdd(blink::IdentifiableSurface surface,
                                     PrivacyBudgetCost budget) {
  return TryAdd(valuation_->equivalence().GetRepresentative(surface), budget);
}

void SurfaceSetWithValuation::AssignWithBudget(
    RepresentativeSurfaceSet&& incoming_container,
    double budget) {
  Assign(std::move(incoming_container));

  if (cost_ <= budget)
    return;

  // In case the budget doesn't accommodate all of `surfaces_`, we'll randomly
  // drop elements until we meet the budget's restrictions.
  auto container = std::move(surfaces_).extract();
  base::RandomBitGenerator g;
  base::ranges::shuffle(container, g);

  auto new_beginning = container.begin();
  for (; new_beginning != container.end() && cost_ > budget;
       cost_ -= valuation_->Cost(*new_beginning), ++new_beginning) {
  }

  surfaces_ = container_type(new_beginning, container.end());
}

void SurfaceSetWithValuation::Assign(
    RepresentativeSurfaceSet&& incoming_container) {
  surfaces_ = std::move(incoming_container);
  cost_ = valuation_->Cost(surfaces_);
}

void SurfaceSetWithValuation::Clear() {
  cost_ = 0;
  base::STLClearObject(&surfaces_);
}

RepresentativeSurfaceSet SurfaceSetWithValuation::Take() && {
  cost_ = 0;
  return std::move(surfaces_);
}
