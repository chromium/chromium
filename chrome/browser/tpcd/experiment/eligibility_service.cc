// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/eligibility_service.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "content/public/browser/cookie_deprecation_label_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace tpcd::experiment {

EligibilityService::EligibilityService(Profile* profile)
    : profile_(profile),
      pref_service_(profile->GetPrefs()),
      onboarding_service_(
          TrackingProtectionOnboardingFactory::GetForProfile(profile_)) {
  CHECK(base::FeatureList::IsEnabled(
      features::kCookieDeprecationFacilitatedTesting));
  CHECK(pref_service_);
}

EligibilityService::~EligibilityService() = default;

// static
EligibilityService* EligibilityService::Get(Profile* profile) {
  return EligibilityServiceFactory::GetForProfile(profile);
}

void EligibilityService::Shutdown() {
  onboarding_service_ = nullptr;
}

void EligibilityService::OnClientEligibilityChanged(bool is_eligible) {
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

  // Update the eligibility for the onboarding UX flow.
  if (onboarding_service_) {
    if (is_eligible) {
      onboarding_service_->MaybeMarkEligible();
    } else {
      onboarding_service_->MaybeMarkIneligible();
    }
  }
}

}  // namespace tpcd::experiment
