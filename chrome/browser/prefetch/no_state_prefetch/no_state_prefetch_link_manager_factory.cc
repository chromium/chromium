// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_link_manager_factory.h"

#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_link_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"

namespace prerender {

// static
NoStatePrefetchLinkManager*
NoStatePrefetchLinkManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<NoStatePrefetchLinkManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
NoStatePrefetchLinkManagerFactory*
NoStatePrefetchLinkManagerFactory::GetInstance() {
  return base::Singleton<NoStatePrefetchLinkManagerFactory>::get();
}

NoStatePrefetchLinkManagerFactory::NoStatePrefetchLinkManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "NoStatePrefetchLinkManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(prerender::NoStatePrefetchManagerFactory::GetInstance());
}

KeyedService* NoStatePrefetchLinkManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(profile);
  if (!no_state_prefetch_manager)
    return nullptr;
  NoStatePrefetchLinkManager* no_state_prefetch_link_manager =
      new NoStatePrefetchLinkManager(no_state_prefetch_manager);
  return no_state_prefetch_link_manager;
}

content::BrowserContext*
NoStatePrefetchLinkManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace prerender
