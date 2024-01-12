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
  return static_cast<GuestOsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
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
              // There's only the one service instance per login, instead of a
              // new one for off-the-record. This is because users expect their
              // Guest OSs to still work even when off-the-record, and to work
              // with their existing VM instances. For example, when downloading
              // files in Chrome they can still save their files to a Guest OS
              // mount.
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {}

GuestOsServiceFactory::~GuestOsServiceFactory() = default;

std::unique_ptr<KeyedService>
GuestOsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return nullptr;
  return std::make_unique<GuestOsService>(profile);
}

}  // namespace guest_os
