// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/last_tab_standing_tracker_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/permissions/last_tab_standing_tracker.h"
#include "chrome/browser/profiles/profile.h"

LastTabStandingTracker* LastTabStandingTrackerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<LastTabStandingTracker*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

LastTabStandingTrackerFactory* LastTabStandingTrackerFactory::GetInstance() {
  return base::Singleton<LastTabStandingTrackerFactory>::get();
}

LastTabStandingTrackerFactory::LastTabStandingTrackerFactory()
    : ProfileKeyedServiceFactory(
          "LastTabStandingTrackerKeyedService",
          ProfileSelections::BuildForRegularAndIncognito()) {}

LastTabStandingTrackerFactory::~LastTabStandingTrackerFactory() = default;

bool LastTabStandingTrackerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

KeyedService* LastTabStandingTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new LastTabStandingTracker();
}
