// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "components/sync/model/data_type_store_service.h"

namespace ash {
namespace printing {
namespace oauth2 {

// static
AuthorizationZonesManagerFactory*
AuthorizationZonesManagerFactory::GetInstance() {
  static base::NoDestructor<AuthorizationZonesManagerFactory> factory;
  return factory.get();
}

// static
AuthorizationZonesManager*
AuthorizationZonesManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AuthorizationZonesManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AuthorizationZonesManagerFactory::AuthorizationZonesManagerFactory()
    : ProfileKeyedServiceFactory(
          "AuthorizationZonesManagerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

AuthorizationZonesManagerFactory::~AuthorizationZonesManagerFactory() = default;

std::unique_ptr<KeyedService>
AuthorizationZonesManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return AuthorizationZonesManager::Create(
      g_browser_process->local_state(), profile,
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());
}

}  // namespace oauth2
}  // namespace printing
}  // namespace ash
