// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/app_discovery_service_factory.h"

#include <memory>

#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/profiles/profile.h"

namespace {
static constexpr const char* kAppDiscoveryService = "AppDiscoveryService";
}  // namespace

namespace apps {

// static
AppDiscoveryService* AppDiscoveryServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AppDiscoveryService*>(
      AppDiscoveryServiceFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
AppDiscoveryServiceFactory* AppDiscoveryServiceFactory::GetInstance() {
  return base::Singleton<AppDiscoveryServiceFactory>::get();
}

AppDiscoveryServiceFactory::AppDiscoveryServiceFactory()
    : ProfileKeyedServiceFactory(
          kAppDiscoveryService,
          // Service is available in all modes (kiosk, guest, incognito,
          // all profile types, etc.).
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

AppDiscoveryServiceFactory::~AppDiscoveryServiceFactory() = default;

KeyedService* AppDiscoveryServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppDiscoveryService(Profile::FromBrowserContext(context));
}

}  // namespace apps
