// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/power_bookmarks/power_bookmark_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"

// static
power_bookmarks::PowerBookmarkService*
PowerBookmarkServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<power_bookmarks::PowerBookmarkService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PowerBookmarkServiceFactory* PowerBookmarkServiceFactory::GetInstance() {
  return base::Singleton<PowerBookmarkServiceFactory>::get();
}

PowerBookmarkServiceFactory::PowerBookmarkServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PowerBookmarkService",
          BrowserContextDependencyManager::GetInstance()) {}

PowerBookmarkServiceFactory::~PowerBookmarkServiceFactory() = default;

KeyedService* PowerBookmarkServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new power_bookmarks::PowerBookmarkService();
}
