// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/privacy_policy_insights_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/privacy_policy_insights_service.h"

// static
page_info::PrivacyPolicyInsightsService* PrivacyPolicyInsightsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<page_info::PrivacyPolicyInsightsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}
// static
PrivacyPolicyInsightsServiceFactory* PrivacyPolicyInsightsServiceFactory::GetInstance() {
  static base::NoDestructor<PrivacyPolicyInsightsServiceFactory> factory;
  return factory.get();
}

PrivacyPolicyInsightsServiceFactory::PrivacyPolicyInsightsServiceFactory()
    : ProfileKeyedServiceFactory(
          "PrivacyPolicyInsightsServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

PrivacyPolicyInsightsServiceFactory::~PrivacyPolicyInsightsServiceFactory() = default;

// BrowserContextKeyedServiceFactory:
std::unique_ptr<KeyedService>
PrivacyPolicyInsightsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  if (!base::FeatureList::IsEnabled(page_info::kPrivacyPolicyInsights)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);

  auto* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide) {
    return nullptr;
  }

  return std::make_unique<page_info::PrivacyPolicyInsightsService>(
      optimization_guide, profile->IsOffTheRecord(), profile->GetPrefs());
}

bool PrivacyPolicyInsightsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // This service needs to be created at startup in order to register its
  // OptimizationType with OptimizationGuideDecider.
  return true;
}
