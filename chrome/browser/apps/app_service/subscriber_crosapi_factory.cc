// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/subscriber_crosapi_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/subscriber_crosapi.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace apps {

// static
SubscriberCrosapi* SubscriberCrosapiFactory::GetForProfile(Profile* profile) {
  return static_cast<SubscriberCrosapi*>(
      SubscriberCrosapiFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
SubscriberCrosapiFactory* SubscriberCrosapiFactory::GetInstance() {
  return base::Singleton<SubscriberCrosapiFactory>::get();
}

// static
void SubscriberCrosapiFactory::ShutDownForTesting(
    content::BrowserContext* context) {
  auto* factory = GetInstance();
  factory->BrowserContextShutdown(context);
  factory->BrowserContextDestroyed(context);
}

SubscriberCrosapiFactory::SubscriberCrosapiFactory()
    : BrowserContextKeyedServiceFactory(
          "SubscriberCrosapi",
          BrowserContextDependencyManager::GetInstance()) {
  // The Lacros app service proxy that will talk with the
  // the SubscriberCrosapi will be created by AppServiceProxyFactory.
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

KeyedService* SubscriberCrosapiFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SubscriberCrosapi(Profile::FromBrowserContext(context));
}

content::BrowserContext* SubscriberCrosapiFactory::GetBrowserContextToUse(
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
