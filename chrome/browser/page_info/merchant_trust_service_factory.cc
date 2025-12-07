// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/merchant_trust_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_info/merchant_trust_service_delegate.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/merchant_trust_service.h"

// static
page_info::MerchantTrustService* MerchantTrustServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<page_info::MerchantTrustService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}
// static
MerchantTrustServiceFactory* MerchantTrustServiceFactory::GetInstance() {
  static base::NoDestructor<MerchantTrustServiceFactory> factory;
  return factory.get();
}

MerchantTrustServiceFactory::MerchantTrustServiceFactory()
    : ProfileKeyedServiceFactory(
          "MerchantTrustServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

MerchantTrustServiceFactory::~MerchantTrustServiceFactory() = default;

// BrowserContextKeyedServiceFactory:
std::unique_ptr<KeyedService>
MerchantTrustServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  auto* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide) {
    return nullptr;
  }

  return std::make_unique<page_info::MerchantTrustService>(
      std::make_unique<MerchantTrustServiceDelegate>(profile),
      optimization_guide, profile->IsOffTheRecord(), profile->GetPrefs());
}

bool MerchantTrustServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // This service needs to be created at startup in order to register its
  // OptimizationType with OptimizationGuideDecider.
  return true;
}
