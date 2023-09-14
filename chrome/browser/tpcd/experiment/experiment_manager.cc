// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/experiment_manager.h"

namespace tpcd::experiment {

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
