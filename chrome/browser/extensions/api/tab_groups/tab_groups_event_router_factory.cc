// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router.h"
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
    : ProfileKeyedServiceFactory(
          "TabGroupsEventRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(EventRouterFactory::GetInstance());
}

std::unique_ptr<KeyedService>
TabGroupsEventRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<TabGroupsEventRouter>(context);
}

bool TabGroupsEventRouterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
