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
  return base::Singleton<NoStatePrefetchLinkManagerFactory>::get();
}

NoStatePrefetchLinkManagerFactory::NoStatePrefetchLinkManagerFactory()
    : ProfileKeyedServiceFactory(
          "NoStatePrefetchLinkManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(NoStatePrefetchManagerFactory::GetInstance());
}

KeyedService* NoStatePrefetchLinkManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(context);
  if (!no_state_prefetch_manager)
    return nullptr;
  NoStatePrefetchLinkManager* no_state_prefetch_link_manager =
      new NoStatePrefetchLinkManager(no_state_prefetch_manager);
  return no_state_prefetch_link_manager;
}

}  // namespace prerender
