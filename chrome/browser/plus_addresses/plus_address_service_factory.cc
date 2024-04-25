// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plus_addresses/plus_address_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/plus_addresses/affiliations/plus_address_affiliation_source_adapter.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_http_client_impl.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
plus_addresses::PlusAddressService*
PlusAddressServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<plus_addresses::PlusAddressService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

PlusAddressServiceFactory* PlusAddressServiceFactory::GetInstance() {
  static base::NoDestructor<PlusAddressServiceFactory> instance;
  return instance.get();
}

/* static */
ProfileSelections PlusAddressServiceFactory::CreateProfileSelections() {
  // Feature not enabled? Don't create any service instances.
  if (!base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressesEnabled)) {
    return ProfileSelections::BuildNoProfilesSelected();
  }
  // Otherwise, exclude system accounts and guest accounts, otherwise use one
  // instance.
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

PlusAddressServiceFactory::PlusAddressServiceFactory()
    : ProfileKeyedServiceFactory("PlusAddressService",
                                 CreateProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
  DependsOn(AffiliationServiceFactory::GetInstance());
}

PlusAddressServiceFactory::~PlusAddressServiceFactory() = default;

std::unique_ptr<KeyedService>
PlusAddressServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // In Ash, GuestSession uses Regular Profile, for which we will try to create
  // the service. Do not create the service for Guest Session.
  if (profile->IsGuestSession()) {
    return nullptr;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  std::unique_ptr<plus_addresses::PlusAddressService> plus_address_service =
      std::make_unique<plus_addresses::PlusAddressService>(
          identity_manager, profile->GetPrefs(),
          std::make_unique<plus_addresses::PlusAddressHttpClientImpl>(
              identity_manager, profile->GetURLLoaderFactory()),
          WebDataServiceFactory::GetPlusAddressWebDataForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS));

  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressAffiliations)) {
    AffiliationServiceFactory::GetForProfile(profile)->RegisterSource(
        std::make_unique<plus_addresses::PlusAddressAffiliationSourceAdapter>(
            plus_address_service.get()));
  }

  return plus_address_service;
}

// Create this service when the profile is created to support populating the
// local map of plus addresses before the user interacts with the feature.
bool PlusAddressServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
