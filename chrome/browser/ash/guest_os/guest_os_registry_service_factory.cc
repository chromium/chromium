// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"

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
    : BrowserContextKeyedServiceFactory(
          "GuestOsRegistryService",
          BrowserContextDependencyManager::GetInstance()) {}

GuestOsRegistryServiceFactory::~GuestOsRegistryServiceFactory() = default;

KeyedService* GuestOsRegistryServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new GuestOsRegistryService(profile);
}

}  // namespace guest_os
