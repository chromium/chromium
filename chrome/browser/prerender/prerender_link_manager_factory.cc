// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_link_manager_factory.h"

#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prerender/browser/prerender_link_manager.h"
#include "components/prerender/browser/prerender_manager.h"

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
  DependsOn(prerender::PrerenderManagerFactory::GetInstance());
}

KeyedService* PrerenderLinkManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(profile);
  if (!prerender_manager)
    return NULL;
  PrerenderLinkManager* prerender_link_manager =
      new PrerenderLinkManager(prerender_manager);
  return prerender_link_manager;
}

content::BrowserContext* PrerenderLinkManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace prerender
