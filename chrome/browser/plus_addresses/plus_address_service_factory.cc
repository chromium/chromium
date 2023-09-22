// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_client.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

/* static */
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
  if (!base::FeatureList::IsEnabled(plus_addresses::kFeature)) {
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
  return std::make_unique<plus_addresses::PlusAddressService>(
      identity_manager, profile->GetPrefs(),
      plus_addresses::PlusAddressClient(identity_manager,
                                        profile->GetURLLoaderFactory()));
}

// Create this service when the profile is created to support populating the
// local map of plus addresses before the user interacts with the feature.
bool PlusAddressServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
