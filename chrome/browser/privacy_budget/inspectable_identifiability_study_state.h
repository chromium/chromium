// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_INSPECTABLE_IDENTIFIABILITY_STUDY_STATE_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_INSPECTABLE_IDENTIFIABILITY_STUDY_STATE_H_

#include "chrome/browser/privacy_budget/identifiability_study_state.h"

namespace test_utils {

// This class is a friend of IdentifiabilityStudyState and can reach into the
// internals. Use this as a last resort.
class InspectableIdentifiabilityStudyState : public IdentifiabilityStudyState {
 public:
  using IdentifiabilityStudyState::IdentifiableSurfaceSet;
  using IdentifiabilityStudyState::IdentifiableSurfaceTypeSet;

  explicit InspectableIdentifiabilityStudyState(PrefService* pref_service)
      : IdentifiabilityStudyState(pref_service) {}

  const IdentifiableSurfaceSet& active_surfaces() const {
    return active_surfaces_;
  }
  const IdentifiableSurfaceSet& retired_surfaces() const {
    return retired_surfaces_;
  }
  int max_active_surfaces() const { return max_active_surfaces_; }
  int surface_selection_rate() const { return surface_selection_rate_; }
  uint64_t prng_seed() const { return prng_seed_; }
};

}  // namespace test_utils

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_INSPECTABLE_IDENTIFIABILITY_STUDY_STATE_H_
