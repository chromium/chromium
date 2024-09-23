// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/eligibility_service.h"

#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "chrome/browser/tpcd/experiment/experiment_manager.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "content/public/browser/cookie_deprecation_label_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace tpcd::experiment {

inline void UmaHistogramProfileEligibilityMismatch(
    bool is_profile_eligible,
    bool is_client_in_experiment) {
  if (is_client_in_experiment && is_profile_eligible) {
    base::UmaHistogramEnumeration(
        ProfileEligibilityMismatchHistogramName,
        ProfileEligibilityMismatch::kEligibleProfileInExperiment);
  }
  if (!is_client_in_experiment && !is_profile_eligible) {
    base::UmaHistogramEnumeration(
        ProfileEligibilityMismatchHistogramName,
        ProfileEligibilityMismatch::kIneligibleProfileNotInExperiment);
  }
  if (is_client_in_experiment && !is_profile_eligible) {
    base::UmaHistogramEnumeration(
        ProfileEligibilityMismatchHistogramName,
        ProfileEligibilityMismatch::kIneligibleProfileInExperiment);
  }
  if (!is_client_in_experiment && is_profile_eligible) {
    base::UmaHistogramEnumeration(
        ProfileEligibilityMismatchHistogramName,
        ProfileEligibilityMismatch::kEligibleProfileNotInExperiment);
  }
}

EligibilityService::EligibilityService(
    Profile* profile,
    privacy_sandbox::TrackingProtectionOnboarding*
        tracking_protection_onboarding,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    ExperimentManager* experiment_manager)
    : profile_(profile),
      onboarding_service_(tracking_protection_onboarding),
      privacy_sandbox_settings_(privacy_sandbox_settings),
      experiment_manager_(experiment_manager) {
  CHECK(base::FeatureList::IsEnabled(
      features::kCookieDeprecationFacilitatedTesting));
  CHECK(experiment_manager_);
  CHECK(privacy_sandbox_settings_);

  profile_eligibility_ = ProfileEligibility();
  BroadcastProfileEligibility();
}

EligibilityService::~EligibilityService() = default;

// static
EligibilityService* EligibilityService::Get(Profile* profile) {
  return EligibilityServiceFactory::GetForProfile(profile);
}

void EligibilityService::Shutdown() {
  if (onboarding_service_) {
    onboarding_service_ = nullptr;
  }
  privacy_sandbox_settings_ = nullptr;
}

void EligibilityService::BroadcastProfileEligibility() {
  CHECK(profile_eligibility_.has_value());
  std::optional<bool> is_client_eligible =
      experiment_manager_->IsClientEligible();
  if (is_client_eligible.has_value()) {
    MarkProfileEligibility(is_client_eligible.value());
    return;
  }

  base::UmaHistogramEnumeration(
      "PrivacySandbox.CookieDeprecationFacilitatedTesting."
      "ReasonForEligibilityStoredInPrefs",
      profile_eligibility_->reason());

  experiment_manager_->SetClientEligibility(
      profile_eligibility_->is_eligible(),
      base::BindOnce(&EligibilityService::MarkProfileEligibility,
                     weak_factory_.GetWeakPtr()));
}

void EligibilityService::MarkProfileEligibility(bool is_client_eligible) {
  // Record when profile eligiblity and client eligiblity matches and
  // mismatches.
  UmaHistogramProfileEligibilityMismatch(profile_eligibility_->is_eligible(),
                                         is_client_eligible);
  base::UmaHistogramEnumeration(
      "PrivacySandbox.CookieDeprecationFacilitatedTesting."
      "ReasonForComputedEligibilityForProfile",
      profile_eligibility_->reason());

  UpdateCookieDeprecationLabel();

  // Update the eligibility for the onboarding UX flow.
  if (onboarding_service_) {
    if (kDisable3PCookies.Get()) {
      MaybeNotifyManagerTrackingProtectionOnboarded(
          onboarding_service_->GetOnboardingStatus());
    } else if (kEnableSilentOnboarding.Get()) {
      MaybeNotifyManagerTrackingProtectionSilentOnboarded(
          onboarding_service_->GetSilentOnboardingStatus());
    }
  }
}

privacy_sandbox::TpcdExperimentEligibility
EligibilityService::ProfileEligibility() {
  CHECK(privacy_sandbox_settings_);
  return privacy_sandbox_settings_
      ->GetCookieDeprecationExperimentCurrentEligibility();
}

void EligibilityService::UpdateCookieDeprecationLabel() {
  // For each storage partition, update the cookie deprecation label to the
  // updated value from the CookieDeprecationLabelManager.
  profile_->ForEachLoadedStoragePartition(
      [](content::StoragePartition* storage_partition) {
        if (auto* cookie_deprecation_label_manager =
                storage_partition->GetCookieDeprecationLabelManager()) {
          storage_partition->GetNetworkContext()->SetCookieDeprecationLabel(
              cookie_deprecation_label_manager->GetValue().value_or(""));
        }
      });
}

void EligibilityService::MaybeNotifyManagerTrackingProtectionOnboarded(
    privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus
        onboarding_status) {
  if (onboarding_status == privacy_sandbox::TrackingProtectionOnboarding::
                               OnboardingStatus::kOnboarded) {
    experiment_manager_->NotifyProfileTrackingProtectionOnboarded();
  }
}

void EligibilityService::MaybeNotifyManagerTrackingProtectionSilentOnboarded(
    privacy_sandbox::TrackingProtectionOnboarding::SilentOnboardingStatus
        onboarding_status) {
  if (onboarding_status == privacy_sandbox::TrackingProtectionOnboarding::
                               SilentOnboardingStatus::kOnboarded) {
    experiment_manager_->NotifyProfileTrackingProtectionOnboarded();
  }
}

}  // namespace tpcd::experiment
