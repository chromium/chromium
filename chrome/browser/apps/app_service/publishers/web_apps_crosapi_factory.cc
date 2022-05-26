// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace apps {

// static
WebAppsCrosapi* WebAppsCrosapiFactory::GetForProfile(Profile* profile) {
  return static_cast<WebAppsCrosapi*>(
      WebAppsCrosapiFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
WebAppsCrosapiFactory* WebAppsCrosapiFactory::GetInstance() {
  return base::Singleton<WebAppsCrosapiFactory>::get();
}

// static
void WebAppsCrosapiFactory::ShutDownForTesting(
    content::BrowserContext* context) {
  auto* factory = GetInstance();
  factory->BrowserContextShutdown(context);
  factory->BrowserContextDestroyed(context);
}

WebAppsCrosapiFactory::WebAppsCrosapiFactory()
    : BrowserContextKeyedServiceFactory(
          "WebAppsCrosapi",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AppServiceProxyFactory::GetInstance());
}

KeyedService* WebAppsCrosapiFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new WebAppsCrosapi(AppServiceProxyFactory::GetForProfile(
      Profile::FromBrowserContext(context)));
}

content::BrowserContext* WebAppsCrosapiFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return nullptr;
  }

  // Use OTR profile for Guest Session.
  if (profile->IsGuestSession()) {
    return profile->IsOffTheRecord()
               ? chrome::GetBrowserContextOwnInstanceInIncognito(context)
               : nullptr;
  }

  return BrowserContextKeyedServiceFactory::GetBrowserContextToUse(context);
}

}  // namespace apps
