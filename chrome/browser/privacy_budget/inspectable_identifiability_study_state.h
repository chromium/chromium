// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_INSPECTABLE_IDENTIFIABILITY_STUDY_STATE_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_INSPECTABLE_IDENTIFIABILITY_STUDY_STATE_H_

#include "base/containers/flat_set.h"
#include "chrome/browser/privacy_budget/identifiability_study_group_settings.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "chrome/browser/privacy_budget/representative_surface_set.h"
#include "chrome/browser/privacy_budget/surface_set_valuation.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace test_utils {

// This class is a friend of IdentifiabilityStudyState and can reach into the
// internals.
class InspectableIdentifiabilityStudyState : public IdentifiabilityStudyState {
 public:
  explicit InspectableIdentifiabilityStudyState(PrefService* pref_service);

  const SurfaceSetValuation& valuation() const { return valuation_; }
  const SurfaceSetWithValuation& active_surfaces() const {
    return active_surfaces_;
  }
  const OrderPreservingSet<blink::IdentifiableSurface>& seen_surfaces() const {
    return seen_surfaces_;
  }
  const base::flat_set<OffsetType>& selected_offsets() const {
    return selected_offsets_;
  }
  int active_surface_budget() const { return active_surface_budget_; }
  int selected_block_offset() const { return selected_block_offset_; }
  const IdentifiabilityStudyGroupSettings& group_settings() const {
    return settings_;
  }
  bool meta_experiment_active() const { return meta_experiment_active_; }

  void SelectAllOffsetsForTesting();

  // These are exposed for testing.
  using IdentifiabilityStudyState::AdjustForDroppedOffsets;
  using IdentifiabilityStudyState::kMaxSelectedSurfaceOffset;
  using IdentifiabilityStudyState::StripDisallowedSurfaces;
};

}  // namespace test_utils

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_INSPECTABLE_IDENTIFIABILITY_STUDY_STATE_H_
