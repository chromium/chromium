// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_link_manager_factory.h"

#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
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
  static base::NoDestructor<NoStatePrefetchLinkManagerFactory> instance;
  return instance.get();
}

NoStatePrefetchLinkManagerFactory::NoStatePrefetchLinkManagerFactory()
    : ProfileKeyedServiceFactory(
          "NoStatePrefetchLinkManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(NoStatePrefetchManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
NoStatePrefetchLinkManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(context);
  if (!no_state_prefetch_manager)
    return nullptr;
  return std::make_unique<NoStatePrefetchLinkManager>(no_state_prefetch_manager);
}

}  // namespace prerender
