// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/closed_tab_cache_service_factory.h"

#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

ClosedTabCacheServiceFactory::ClosedTabCacheServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ClosedTabCacheService",
          BrowserContextDependencyManager::GetInstance()) {}

// static
ClosedTabCacheService* ClosedTabCacheServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ClosedTabCacheService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ClosedTabCacheServiceFactory* ClosedTabCacheServiceFactory::GetInstance() {
  return base::Singleton<ClosedTabCacheServiceFactory>::get();
}

KeyedService* ClosedTabCacheServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord())
    return nullptr;
  return new ClosedTabCacheService(static_cast<Profile*>(context));
}

bool ClosedTabCacheServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
