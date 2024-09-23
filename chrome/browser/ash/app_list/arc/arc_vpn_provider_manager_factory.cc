// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_vpn_provider_manager_factory.h"

#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_vpn_provider_manager.h"
#include "content/public/browser/browser_context.h"

namespace app_list {

// static
ArcVpnProviderManager* ArcVpnProviderManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcVpnProviderManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ArcVpnProviderManagerFactory* ArcVpnProviderManagerFactory::GetInstance() {
  static base::NoDestructor<ArcVpnProviderManagerFactory> instance;
  return instance.get();
}

ArcVpnProviderManagerFactory::ArcVpnProviderManagerFactory()
    : ProfileKeyedServiceFactory(
          "ArcVpnProviderManager",
          // This matches the logic in ExtensionSyncServiceFactory, which uses
          // the original browser context.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ArcAppListPrefsFactory::GetInstance());
}

ArcVpnProviderManagerFactory::~ArcVpnProviderManagerFactory() = default;

std::unique_ptr<KeyedService>
ArcVpnProviderManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return ArcVpnProviderManager::Create(context);
}

}  // namespace app_list
