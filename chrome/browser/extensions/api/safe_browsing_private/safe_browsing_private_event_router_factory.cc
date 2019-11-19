// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
SafeBrowsingPrivateEventRouter*
SafeBrowsingPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<SafeBrowsingPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SafeBrowsingPrivateEventRouterFactory*
SafeBrowsingPrivateEventRouterFactory::GetInstance() {
  return base::Singleton<SafeBrowsingPrivateEventRouterFactory>::get();
}

SafeBrowsingPrivateEventRouterFactory::SafeBrowsingPrivateEventRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "SafeBrowsingPrivateEventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(IdentityManagerFactory::GetInstance());
}

SafeBrowsingPrivateEventRouterFactory::
    ~SafeBrowsingPrivateEventRouterFactory() {}

KeyedService* SafeBrowsingPrivateEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SafeBrowsingPrivateEventRouter(context);
}

content::BrowserContext*
SafeBrowsingPrivateEventRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

bool SafeBrowsingPrivateEventRouterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool SafeBrowsingPrivateEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
