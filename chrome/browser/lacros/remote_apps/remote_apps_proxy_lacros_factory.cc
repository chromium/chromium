// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros_factory.h"

#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"

namespace chromeos {

// static
RemoteAppsProxyLacros* RemoteAppsProxyLacrosFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<RemoteAppsProxyLacros*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /* create= */ true));
}

// static
RemoteAppsProxyLacrosFactory* RemoteAppsProxyLacrosFactory::GetInstance() {
  static base::NoDestructor<RemoteAppsProxyLacrosFactory> instance;
  return instance.get();
}

RemoteAppsProxyLacrosFactory::RemoteAppsProxyLacrosFactory()
    : ProfileKeyedServiceFactory(
          "RemoteAppsProxyLacros",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(extensions::EventRouterFactory::GetInstance());
}

RemoteAppsProxyLacrosFactory::~RemoteAppsProxyLacrosFactory() = default;

std::unique_ptr<KeyedService>
RemoteAppsProxyLacrosFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  if (!chromeos::LacrosService::Get()
           ->IsAvailable<
               chromeos::remote_apps::mojom::RemoteAppsLacrosBridge>()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  return std::make_unique<RemoteAppsProxyLacros>(profile);
}

bool RemoteAppsProxyLacrosFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool RemoteAppsProxyLacrosFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace chromeos
