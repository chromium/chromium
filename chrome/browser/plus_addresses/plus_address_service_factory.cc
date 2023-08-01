// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"

#include "base/no_destructor.h"

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
  // TODO(crbug.com/1467623): Add identity dependency when it is added to the
  // `PlusAddressService`.
}

PlusAddressServiceFactory::~PlusAddressServiceFactory() = default;

KeyedService* PlusAddressServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // In Ash, GuestSession uses Regular Profile, for which we will try to create
  // the service. Do not create the service for Guest Session.
  if (profile->IsGuestSession()) {
    return nullptr;
  }
  return new plus_addresses::PlusAddressService();
}
