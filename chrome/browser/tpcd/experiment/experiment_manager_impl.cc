// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/experiment_manager_impl.h"

#include <optional>
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
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tpcd_pref_names.h"
#include "components/privacy_sandbox/tpcd_utils.h"
#include "components/variations/synthetic_trials.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace tpcd::experiment {
namespace {

const base::FeatureParam<std::string> kSyntheticTrialGroupOverride{
    &features::kCookieDeprecationFacilitatedTesting,
    "synthetic_trial_group_override", ""};

bool NeedsOnboardingForExperiment() {
  if (!kDisable3PCookies.Get() && !kEnableSilentOnboarding.Get()) {
    return false;
  }

  return kNeedOnboardingForSyntheticTrial.Get();
}

}  // namespace

// static
ExperimentManagerImpl* ExperimentManagerImpl::GetForProfile(Profile* profile) {
  if (!base::FeatureList::IsEnabled(
          features::kCookieDeprecationFacilitatedTesting)) {
    return nullptr;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Ash internal profile should not be accounted for the experiment
  // eligibility, and therefore should not create the experiment manager.
  if (!ash::IsUserBrowserContext(profile)) {
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
    did_version_change_ = true;
  }

  // If client eligibility is already known, do not recompute it.
  if (IsClientEligible().has_value()) {
    // The user must be re-registered to the synthetic trial on restart.
    MaybeUpdateSyntheticTrialRegistration();
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

  if (std::optional<bool> client_is_eligible = IsClientEligible()) {
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
  MaybeUpdateSyntheticTrialRegistration();

  // Run the EligibilityDecisionCallback for every profile that marked its
  // eligibility.
  for (auto& callback : callbacks_) {
    std::move(callback).Run(client_is_eligible_);
  }
  callbacks_.clear();
}

void ExperimentManagerImpl::MaybeUpdateSyntheticTrialRegistration() {
  if (!CanRegisterSyntheticTrial()) {
    return;
  }

  std::optional<bool> is_client_eligible = IsClientEligible();
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

std::optional<bool> ExperimentManagerImpl::IsClientEligible() const {
  if (kForceEligibleForTesting.Get()) {
    return true;
  }

  switch (g_browser_process->local_state()->GetInteger(
      prefs::kTPCDExperimentClientState)) {
    case static_cast<int>(utils::ExperimentState::kEligible):
    case static_cast<int>(utils::ExperimentState::kOnboarded):
      return true;
    case static_cast<int>(utils::ExperimentState::kIneligible):
      return false;
    case static_cast<int>(utils::ExperimentState::kUnknownEligibility):
      return std::nullopt;
    default:
      // invalid
      return false;
  }
}

bool ExperimentManagerImpl::DidVersionChange() const {
  return did_version_change_;
}

void ExperimentManagerImpl::NotifyProfileTrackingProtectionOnboarded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!NeedsOnboardingForExperiment()) {
    return;
  }

  switch (g_browser_process->local_state()->GetInteger(
      prefs::kTPCDExperimentClientState)) {
    case static_cast<int>(utils::ExperimentState::kEligible):
      break;
    case static_cast<int>(utils::ExperimentState::kIneligible):
    case static_cast<int>(utils::ExperimentState::kOnboarded):
      return;
    case static_cast<int>(utils::ExperimentState::kUnknownEligibility):
      if (kForceEligibleForTesting.Get()) {
        break;
      } else {
        return;
      }
    default:
      // invalid
      return;
  }

  g_browser_process->local_state()->SetInteger(
      prefs::kTPCDExperimentClientState,
      static_cast<int>(utils::ExperimentState::kOnboarded));

  MaybeUpdateSyntheticTrialRegistration();
}

bool ExperimentManagerImpl::CanRegisterSyntheticTrial() const {
  switch (g_browser_process->local_state()->GetInteger(
      prefs::kTPCDExperimentClientState)) {
    case static_cast<int>(utils::ExperimentState::kEligible):
      return !NeedsOnboardingForExperiment();
    case static_cast<int>(utils::ExperimentState::kIneligible):
    case static_cast<int>(utils::ExperimentState::kOnboarded):
      return true;
    case static_cast<int>(utils::ExperimentState::kUnknownEligibility):
      if (kForceEligibleForTesting.Get()) {
        return !NeedsOnboardingForExperiment();
      } else {
        return false;
      }
    default:
      // invalid
      return true;
  }
}

}  // namespace tpcd::experiment
