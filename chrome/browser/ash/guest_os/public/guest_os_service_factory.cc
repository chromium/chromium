// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"

#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

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
    : BrowserContextKeyedServiceFactory(
          "GuestOsService",
          BrowserContextDependencyManager::GetInstance()) {}

GuestOsServiceFactory::~GuestOsServiceFactory() = default;

KeyedService* GuestOsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new GuestOsService();
}

}  // namespace guest_os
