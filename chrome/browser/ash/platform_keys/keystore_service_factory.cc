// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/keystore_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/ash/platform_keys/keystore_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"

namespace ash {

// static
KeystoreService* KeystoreServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<KeystoreService*>(
      KeystoreServiceFactory::GetInstance()->GetServiceForBrowserContext(
          context, /*create=*/true));
}

// static
KeystoreServiceFactory* KeystoreServiceFactory::GetInstance() {
  static base::NoDestructor<KeystoreServiceFactory> factory;
  return factory.get();
}

KeystoreServiceFactory::KeystoreServiceFactory()
    : ProfileKeyedServiceFactory(
          "KeystoreServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(platform_keys::PlatformKeysServiceFactory::GetInstance());
  DependsOn(platform_keys::KeyPermissionsServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
KeystoreServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<KeystoreService>(context);
}

}  // namespace ash
