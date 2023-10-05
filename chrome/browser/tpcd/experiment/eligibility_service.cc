// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/eligibility_service.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "chrome/browser/tpcd/experiment/experiment_manager.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "content/public/browser/cookie_deprecation_label_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

EligibilityService::EligibilityService(Profile* profile,
                                       ExperimentManager* experiment_manager)
    : profile_(profile),
      onboarding_service_(
          TrackingProtectionOnboardingFactory::GetForProfile(profile_)),
      experiment_manager_(experiment_manager) {
  CHECK(base::FeatureList::IsEnabled(
      features::kCookieDeprecationFacilitatedTesting));
  CHECK(experiment_manager_);

  is_profile_eligible_ = IsProfileEligible();
  BroadcastProfileEligibility();
}

EligibilityService::~EligibilityService() = default;

// static
EligibilityService* EligibilityService::Get(Profile* profile) {
  return EligibilityServiceFactory::GetForProfile(profile);
}

void EligibilityService::Shutdown() {
  onboarding_service_ = nullptr;
}

void EligibilityService::BroadcastProfileEligibility() {
  absl::optional<bool> is_client_eligible =
      experiment_manager_->IsClientEligible();
  if (is_client_eligible.has_value()) {
    MarkProfileEligibility(is_client_eligible.value());
    return;
  }

  experiment_manager_->SetClientEligibility(
      is_profile_eligible_,
      base::BindOnce(&EligibilityService::MarkProfileEligibility,
                     weak_factory_.GetWeakPtr()));
}

void EligibilityService::MarkProfileEligibility(bool is_client_eligible) {
  // Record when profile eligiblity and client eligiblity matches and
  // mismatches.
  UmaHistogramProfileEligibilityMismatch(is_profile_eligible_,
                                         is_client_eligible);

  // For each storage partition, update the cookie deprecation label to the
  // updated value from the CookieDeprecationLabelManager.
  profile_->ForEachLoadedStoragePartition(
      base::BindRepeating([](content::StoragePartition* storage_partition) {
        if (auto* cookie_deprecation_label_manager =
                storage_partition->GetCookieDeprecationLabelManager()) {
          storage_partition->GetNetworkContext()->SetCookieDeprecationLabel(
              cookie_deprecation_label_manager->GetValue());
        }
      }));

  // Update the eligibility for the onboarding UX flow. Check that the user is
  // in Mode B (kDisable3PCookies is true).
  if (onboarding_service_ && kDisable3PCookies.Get()) {
    if (is_client_eligible) {
      onboarding_service_->MaybeMarkEligible();
    } else {
      onboarding_service_->MaybeMarkIneligible();
    }
  }
}

bool EligibilityService::IsProfileEligible() {
  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile_);
  CHECK(privacy_sandbox_settings);

  return privacy_sandbox_settings
      ->IsCookieDeprecationExperimentCurrentlyEligible();
}

}  // namespace tpcd::experiment
