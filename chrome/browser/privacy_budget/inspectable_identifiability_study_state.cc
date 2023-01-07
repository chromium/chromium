// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/inspectable_identifiability_study_state.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/prefs/pref_service.h"

namespace {

PrefService* ResetGlobalStatePassThru(PrefService* pref_service) {
  IdentifiabilityStudyState::ResetGlobalStudySettingsForTesting();
  return pref_service;
}

}  // namespace

namespace test_utils {

InspectableIdentifiabilityStudyState::InspectableIdentifiabilityStudyState(
    PrefService* pref_service)
    : IdentifiabilityStudyState(ResetGlobalStatePassThru(pref_service)) {}

void InspectableIdentifiabilityStudyState::SelectAllOffsetsForTesting() {
  DCHECK(seen_surfaces_.empty());
  DCHECK(active_surfaces_.Empty());

  base::flat_set<OffsetType>::container_type all_offsets;
  all_offsets.reserve(kMaxSelectedSurfaceOffset + 1);
  for (OffsetType i = 0; i <= kMaxSelectedSurfaceOffset; ++i) {
    all_offsets.push_back(i);
  }
  selected_offsets_.replace(std::move(all_offsets));
  WriteSelectedOffsetsToPrefs();

  CheckInvariants();
}

}  // namespace test_utils
