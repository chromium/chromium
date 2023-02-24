// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"

#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/profiles/profile.h"

namespace guest_os {

// static
GuestOsService* guest_os::GuestOsServiceFactory::GetForProfile(
    Profile* profile) {
  // There's only the one service instance per login, instead of a new one for
  // off-the-record. This is because users expect their Guest OSs to still work
  // even when off-the-record, and to work with their existing VM instances. For
  // example, when downloading files in Chrome they can still save their files
  // to a Guest OS mount.
  return static_cast<GuestOsService*>(
      GetInstance()->GetServiceForBrowserContext(profile->GetOriginalProfile(),
                                                 true));
}

// static
GuestOsServiceFactory* GuestOsServiceFactory::GetInstance() {
  static base::NoDestructor<GuestOsServiceFactory> factory;
  return factory.get();
}

GuestOsServiceFactory::GuestOsServiceFactory()
    : ProfileKeyedServiceFactory(
          "GuestOsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

GuestOsServiceFactory::~GuestOsServiceFactory() = default;

KeyedService* GuestOsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return nullptr;
  return new GuestOsService(profile);
}

}  // namespace guest_os
