// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/apps_fetcher_service/apps_fetcher_service_factory.h"

#include <memory>

#include "chrome/browser/apps/apps_fetcher_service/apps_fetcher_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace {
static constexpr const char* kAppsFetcherService = "AppsFetcherService";
}  // namespace

namespace apps {

// static
AppsFetcherService* AppsFetcherServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AppsFetcherService*>(
      AppsFetcherServiceFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
AppsFetcherServiceFactory* AppsFetcherServiceFactory::GetInstance() {
  return base::Singleton<AppsFetcherServiceFactory>::get();
}

AppsFetcherServiceFactory::AppsFetcherServiceFactory()
    : BrowserContextKeyedServiceFactory(
          kAppsFetcherService,
          BrowserContextDependencyManager::GetInstance()) {}

AppsFetcherServiceFactory::~AppsFetcherServiceFactory() = default;

KeyedService* AppsFetcherServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppsFetcherService(Profile::FromBrowserContext(context));
}

content::BrowserContext* AppsFetcherServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Service is available in all modes (kiosk, guest, incognito,
  // all profile types, etc.).
  return context;
}

}  // namespace apps
