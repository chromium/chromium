// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/app_discovery_service_factory.h"

#include <memory>

#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

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
    : BrowserContextKeyedServiceFactory(
          kAppDiscoveryService,
          BrowserContextDependencyManager::GetInstance()) {}

AppDiscoveryServiceFactory::~AppDiscoveryServiceFactory() = default;

KeyedService* AppDiscoveryServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppDiscoveryService(Profile::FromBrowserContext(context));
}

content::BrowserContext* AppDiscoveryServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Service is available in all modes (kiosk, guest, incognito,
  // all profile types, etc.).
  return context;
}

}  // namespace apps
