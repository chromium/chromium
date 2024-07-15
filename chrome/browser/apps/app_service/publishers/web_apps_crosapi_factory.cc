// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"
#include "chrome/browser/profiles/profile.h"

namespace apps {

// static
WebAppsCrosapi* WebAppsCrosapiFactory::GetForProfile(Profile* profile) {
  return static_cast<WebAppsCrosapi*>(
      WebAppsCrosapiFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
WebAppsCrosapiFactory* WebAppsCrosapiFactory::GetInstance() {
  static base::NoDestructor<WebAppsCrosapiFactory> instance;
  return instance.get();
}

// static
void WebAppsCrosapiFactory::ShutDownForTesting(
    content::BrowserContext* context) {
  auto* factory = GetInstance();
  factory->BrowserContextShutdown(context);
  factory->BrowserContextDestroyed(context);
}

WebAppsCrosapiFactory::WebAppsCrosapiFactory()
    : ProfileKeyedServiceFactory(
          "WebAppsCrosapi",
          ProfileSelections::Builder()
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(AppServiceProxyFactory::GetInstance());
}

std::unique_ptr<KeyedService>
WebAppsCrosapiFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<WebAppsCrosapi>(AppServiceProxyFactory::GetForProfile(
      Profile::FromBrowserContext(context)));
}

}  // namespace apps
