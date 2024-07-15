// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/keystore_service_factory_ash.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"

namespace crosapi {

// static
KeystoreServiceAsh* KeystoreServiceFactoryAsh::GetForBrowserContext(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  if (ash::ProfileHelper::IsPrimaryProfile(profile)) {
    // This is the main KeystoreService that is expected to be used most of the
    // time.
    return crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->keystore_service_ash();
  }

  // These services are supposed to cover multi-sign-in scenario and other less
  // common use cases.
  return static_cast<KeystoreServiceAsh*>(
      KeystoreServiceFactoryAsh::GetInstance()->GetServiceForBrowserContext(
          context, /*create=*/true));
}

// static
KeystoreServiceFactoryAsh* KeystoreServiceFactoryAsh::GetInstance() {
  static base::NoDestructor<KeystoreServiceFactoryAsh> factory;
  return factory.get();
}

KeystoreServiceFactoryAsh::KeystoreServiceFactoryAsh()
    : ProfileKeyedServiceFactory(
          "KeystoreServiceFactoryAsh",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ash::platform_keys::PlatformKeysServiceFactory::GetInstance());
  DependsOn(ash::platform_keys::KeyPermissionsServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
KeystoreServiceFactoryAsh::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<KeystoreServiceAsh>(context);
}

}  // namespace crosapi
