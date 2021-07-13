// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/network_context_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/network_context_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace safe_browsing {

// static
NetworkContextServiceFactory* NetworkContextServiceFactory::GetInstance() {
  return base::Singleton<NetworkContextServiceFactory>::get();
}

// static
NetworkContextService* NetworkContextServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<NetworkContextService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

NetworkContextServiceFactory::NetworkContextServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SafeBrowsingNetworkContextService",
          BrowserContextDependencyManager::GetInstance()) {}

NetworkContextServiceFactory::~NetworkContextServiceFactory() = default;

KeyedService* NetworkContextServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new NetworkContextService(profile);
}

content::BrowserContext* NetworkContextServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace safe_browsing
