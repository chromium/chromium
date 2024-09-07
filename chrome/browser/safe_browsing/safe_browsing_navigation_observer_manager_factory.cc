// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace safe_browsing {

// static
SafeBrowsingNavigationObserverManager*
SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<SafeBrowsingNavigationObserverManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/true));
}

// static
SafeBrowsingNavigationObserverManagerFactory*
SafeBrowsingNavigationObserverManagerFactory::GetInstance() {
  static base::NoDestructor<SafeBrowsingNavigationObserverManagerFactory>
      instance;
  return instance.get();
}

SafeBrowsingNavigationObserverManagerFactory::
    SafeBrowsingNavigationObserverManagerFactory()
    : ProfileKeyedServiceFactory(
          "SafeBrowsingNavigationObserverManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

std::unique_ptr<KeyedService> SafeBrowsingNavigationObserverManagerFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<SafeBrowsingNavigationObserverManager>(
      profile->GetPrefs(),
      profile->GetDefaultStoragePartition()->GetServiceWorkerContext());
}

}  // namespace safe_browsing
