// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/tpcd/experiment/experiment_manager.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/experiment/tpcd_pref_names.h"
#include "chrome/browser/tpcd/experiment/tpcd_utils.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // If client eligibility is already known, there's no work for the manager to
  // do.
  if (IsClientEligible().has_value()) {
    return;
  }

  // `ExperimentManager` is a singleton that lives forever, therefore it's safe
  // to use `base::Unretained()`.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExperimentManager::CaptureEligibilityInLocalStatePref,
                     base::Unretained(this)),
      kDecisionDelayTime.Get());
}

ExperimentManager::~ExperimentManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ExperimentManager::SetClientEligibility(
    bool is_eligible,
    EligibilityDecisionCallback on_eligibility_decision_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (absl::optional<bool> client_is_eligible = IsClientEligible();
      client_is_eligible.has_value()) {
    // If client eligibility is already known, just run callback.
    client_is_eligible_ = *client_is_eligible;
    std::move(on_eligibility_decision_callback).Run(client_is_eligible_);
    return;
  }

  // Wait to run callback when decision is made in
  // `CaptureEligibilityInLocalStatePref`
  client_is_eligible_ = client_is_eligible_ && is_eligible;
  callbacks_.push_back(std::move(on_eligibility_decision_callback));
}

void ExperimentManager::CaptureEligibilityInLocalStatePref() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  g_browser_process->local_state()->SetInteger(
      prefs::kTPCDExperimentClientState,
      client_is_eligible_
          ? static_cast<int>(utils::ExperimentState::kEligible)
          : static_cast<int>(utils::ExperimentState::kIneligible));
  for (auto& callback : callbacks_) {
    std::move(callback).Run(client_is_eligible_);
  }
  callbacks_.clear();
}

absl::optional<bool> ExperimentManager::IsClientEligible() const {
  switch (g_browser_process->local_state()->GetInteger(
      prefs::kTPCDExperimentClientState)) {
    case static_cast<int>(utils::ExperimentState::kEligible):
      return true;
    case static_cast<int>(utils::ExperimentState::kIneligible):
      return false;
    case static_cast<int>(utils::ExperimentState::kUnknownEligibility):
      return absl::nullopt;
    default:
      // invalid
      return false;
  }
}

}  // namespace tpcd::experiment
