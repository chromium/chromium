// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/no_state_prefetch/prerender_link_manager_factory.h"

#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/browser/prerender_link_manager.h"

namespace prerender {

// static
PrerenderLinkManager* PrerenderLinkManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<PrerenderLinkManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
PrerenderLinkManagerFactory* PrerenderLinkManagerFactory::GetInstance() {
  return base::Singleton<PrerenderLinkManagerFactory>::get();
}

PrerenderLinkManagerFactory::PrerenderLinkManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PrerenderLinkmanager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(prerender::NoStatePrefetchManagerFactory::GetInstance());
}

KeyedService* PrerenderLinkManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(profile);
  if (!no_state_prefetch_manager)
    return NULL;
  PrerenderLinkManager* prerender_link_manager =
      new PrerenderLinkManager(no_state_prefetch_manager);
  return prerender_link_manager;
}

content::BrowserContext* PrerenderLinkManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace prerender
