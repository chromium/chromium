// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"

namespace extensions {

// static
TabGroupsEventRouter* TabGroupsEventRouterFactory::Get(
    content::BrowserContext* context) {
  return static_cast<TabGroupsEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, /* create */ true));
}

// static
TabGroupsEventRouterFactory* TabGroupsEventRouterFactory::GetInstance() {
  static base::NoDestructor<TabGroupsEventRouterFactory> g_factory;
  return g_factory.get();
}

TabGroupsEventRouterFactory::TabGroupsEventRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "TabGroupsEventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(EventRouterFactory::GetInstance());
}

KeyedService* TabGroupsEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new TabGroupsEventRouter(context);
}

content::BrowserContext* TabGroupsEventRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool TabGroupsEventRouterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
