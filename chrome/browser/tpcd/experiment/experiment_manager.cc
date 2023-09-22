// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/experiment_manager.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/experiment/tpcd_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"

namespace tpcd::experiment {

// static
ExperimentManager* ExperimentManager::GetInstance() {
  if (!base::FeatureList::IsEnabled(
          features::kCookieDeprecationFacilitatedTesting)) {
    return nullptr;
  }

  static base::NoDestructor<ExperimentManager> instance;
  return instance.get();
}

ExperimentManager::ExperimentManager() {
  CHECK(base::FeatureList::IsEnabled(
      features::kCookieDeprecationFacilitatedTesting));

  PrefService* local_state = g_browser_process->local_state();
  CHECK(local_state);

  const int currentVersion = kVersion.Get();
  if (local_state->GetInteger(prefs::kTPCDExperimentClientStateVersion) !=
      currentVersion) {
    local_state->SetInteger(prefs::kTPCDExperimentClientStateVersion,
                            currentVersion);
    local_state->ClearPref(prefs::kTPCDExperimentClientState);
  }
}

void ExperimentManager::SetClientEligibility(bool is_eligible,
                                             bool is_onboarded) {
  // TODO(trishalfonso@google.com): set the local state pref with appropriate
  // experiment state enum value.
}

void ExperimentManager::RegisterSyntheticTrial() {
  // TODO(trishalfonso@google.com): Register the client for the appropriate arm
  // of the Mode B synthetic trial based on feature flags and params.
}

void ExperimentManager::UnregisterSyntheticTrial() {
  // TODO(trishalfonos@google.com): Once synthetic trial is set up, register the
  // current client for "invalidated" trial arm here.
}

bool ExperimentManager::isClientEligible() {
  // TODO(trishalfonso@google.com): Add checks for if the client was installed <
  // 30 days ago and if the client is on an Android device with PWAs installed.
  return false;
}

}  // namespace tpcd::experiment
