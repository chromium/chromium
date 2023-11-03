// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/experiment_manager_impl.h"

#include <string>
#include <utility>

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
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/experiment/tpcd_pref_names.h"
#include "chrome/browser/tpcd/experiment/tpcd_utils.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/synthetic_trials.h"
#include "content/public/common/content_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace tpcd::experiment {
namespace {

const base::FeatureParam<std::string> kSyntheticTrialGroupOverride{
    &features::kCookieDeprecationFacilitatedTesting,
    "synthetic_trial_group_override", ""};

}  // namespace

// TODO(b/302798031): This flag is needed to deflake
// ExperimentManagerImplSyntheticTrialTest on CQ. Remove once test is fixed.
const base::FeatureParam<bool> kForceProfilesEligibleForTesting{
    &features::kCookieDeprecationFacilitatedTesting, "force_profiles_eligible",
    false};

// static
ExperimentManagerImpl* ExperimentManagerImpl::GetForProfile(Profile* profile) {
  if (!base::FeatureList::IsEnabled(
          features::kCookieDeprecationFacilitatedTesting)) {
    return nullptr;
  }

  if (!features::kCookieDeprecationFacilitatedTestingEnableOTRProfiles.Get() &&
      (profile->IsOffTheRecord() || profile->IsGuestSession())) {
    return nullptr;
  }

  return GetInstance();
}

// static
ExperimentManagerImpl* ExperimentManagerImpl::GetInstance() {
  static base::NoDestructor<ExperimentManagerImpl> instance;
  return instance.get();
}

ExperimentManagerImpl::ExperimentManagerImpl() {
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

  // If client eligibility is already known, do not recompute it.
  if (IsClientEligible().has_value()) {
    // The user must be re-registered to the synthetic trial on restart.
    UpdateSyntheticTrialRegistration();
    return;
  }

  // `ExperimentManager` is a singleton that lives forever, therefore it's safe
  // to use `base::Unretained()`.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExperimentManagerImpl::CaptureEligibilityInLocalStatePref,
                     base::Unretained(this)),
      kDecisionDelayTime.Get());
}

ExperimentManagerImpl::~ExperimentManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ExperimentManagerImpl::SetClientEligibility(
    bool is_eligible,
    EligibilityDecisionCallback on_eligibility_decision_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (absl::optional<bool> client_is_eligible = IsClientEligible()) {
    // If client eligibility is already known, just run callback.
    client_is_eligible_ = *client_is_eligible;
    std::move(on_eligibility_decision_callback).Run(client_is_eligible_);
    return;
  }

  // Wait to run callback when decision is made in
  // `CaptureEligibilityInLocalStatePref`
  if (!kForceProfilesEligibleForTesting.Get()) {
    client_is_eligible_ = client_is_eligible_ && is_eligible;
  }
  callbacks_.push_back(std::move(on_eligibility_decision_callback));
}

void ExperimentManagerImpl::CaptureEligibilityInLocalStatePref() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Update kTPCDExperimentClientState in the local state prefs.
  g_browser_process->local_state()->SetInteger(
      prefs::kTPCDExperimentClientState,
      client_is_eligible_
          ? static_cast<int>(utils::ExperimentState::kEligible)
          : static_cast<int>(utils::ExperimentState::kIneligible));

  // Register or unregister for the synthetic trial based on the new
  // eligibility local state pref.
  UpdateSyntheticTrialRegistration();

  // Run the EligibilityDecisionCallback for every profile that marked its
  // eligibility.
  for (auto& callback : callbacks_) {
    std::move(callback).Run(client_is_eligible_);
  }
  callbacks_.clear();
}

void ExperimentManagerImpl::UpdateSyntheticTrialRegistration() {
  absl::optional<bool> is_client_eligible = IsClientEligible();
  CHECK(is_client_eligible.has_value());

  std::string eligible_group_name =
      !kSyntheticTrialGroupOverride.Get().empty()
          ? kSyntheticTrialGroupOverride.Get()
          : features::kCookieDeprecationLabel.Get();
  std::string group_name = *is_client_eligible
                               ? eligible_group_name
                               : kSyntheticTrialInvalidGroupName;
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kSyntheticTrialName, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

absl::optional<bool> ExperimentManagerImpl::IsClientEligible() const {
  if (kForceEligibleForTesting.Get()) {
    return true;
  }

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
