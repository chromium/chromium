// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace guest_os {

// static
GuestOsRegistryService* guest_os::GuestOsRegistryServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GuestOsRegistryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GuestOsRegistryServiceFactory* GuestOsRegistryServiceFactory::GetInstance() {
  static base::NoDestructor<GuestOsRegistryServiceFactory> factory;
  return factory.get();
}

GuestOsRegistryServiceFactory::GuestOsRegistryServiceFactory()
    : ProfileKeyedServiceFactory(
          "GuestOsRegistryService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {}

GuestOsRegistryServiceFactory::~GuestOsRegistryServiceFactory() = default;

std::unique_ptr<KeyedService>
GuestOsRegistryServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<GuestOsRegistryService>(profile);
}

}  // namespace guest_os
