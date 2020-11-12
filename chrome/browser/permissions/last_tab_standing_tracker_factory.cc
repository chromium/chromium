// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/last_tab_standing_tracker_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/permissions/last_tab_standing_tracker.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

LastTabStandingTracker* LastTabStandingTrackerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<LastTabStandingTracker*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

LastTabStandingTrackerFactory* LastTabStandingTrackerFactory::GetInstance() {
  return base::Singleton<LastTabStandingTrackerFactory>::get();
}

LastTabStandingTrackerFactory::LastTabStandingTrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "LastTabStandingTrackerKeyedService",
          BrowserContextDependencyManager::GetInstance()) {}

LastTabStandingTrackerFactory::~LastTabStandingTrackerFactory() = default;

bool LastTabStandingTrackerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

KeyedService* LastTabStandingTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new LastTabStandingTracker();
}

content::BrowserContext* LastTabStandingTrackerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
