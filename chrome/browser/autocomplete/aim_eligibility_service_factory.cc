// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/autocomplete/aim_eligibility_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
AimEligibilityService* AimEligibilityServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AimEligibilityService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AimEligibilityServiceFactory* AimEligibilityServiceFactory::GetInstance() {
  static base::NoDestructor<AimEligibilityServiceFactory> instance;
  return instance.get();
}

AimEligibilityServiceFactory::AimEligibilityServiceFactory()
    : ProfileKeyedServiceFactory(
          "AimEligibilityService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

AimEligibilityServiceFactory::~AimEligibilityServiceFactory() = default;

std::unique_ptr<KeyedService>
AimEligibilityServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<AimEligibilityService>(
      profile->GetPrefs(), TemplateURLServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}
