// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/eligibility_service.h"
#include "chrome/browser/tpcd/experiment/experiment_manager.h"
#include "content/public/common/content_features.h"

namespace tpcd::experiment {

// static
EligibilityService* EligibilityServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<EligibilityService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
EligibilityServiceFactory* EligibilityServiceFactory::GetInstance() {
  static base::NoDestructor<EligibilityServiceFactory> factory;
  return factory.get();
}

EligibilityServiceFactory::EligibilityServiceFactory()
    : ProfileKeyedServiceFactory(
          "EligibilityServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(TrackingProtectionOnboardingFactory::GetInstance());
}

bool EligibilityServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
EligibilityServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          features::kCookieDeprecationFacilitatedTesting)) {
    return nullptr;
  }
  return std::make_unique<EligibilityService>(
      Profile::FromBrowserContext(context), ExperimentManager::GetInstance());
}

}  // namespace tpcd::experiment
