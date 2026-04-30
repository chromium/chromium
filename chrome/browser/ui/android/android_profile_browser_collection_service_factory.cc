// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/android_profile_browser_collection_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/android_profile_browser_collection_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
AndroidProfileBrowserCollectionService*
AndroidProfileBrowserCollectionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AndroidProfileBrowserCollectionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AndroidProfileBrowserCollectionServiceFactory*
AndroidProfileBrowserCollectionServiceFactory::GetInstance() {
  static base::NoDestructor<AndroidProfileBrowserCollectionServiceFactory>
      instance;
  return instance.get();
}

AndroidProfileBrowserCollectionServiceFactory::
    AndroidProfileBrowserCollectionServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AndroidProfileBrowserCollectionService",
          BrowserContextDependencyManager::GetInstance()) {}

AndroidProfileBrowserCollectionServiceFactory::
    ~AndroidProfileBrowserCollectionServiceFactory() = default;

std::unique_ptr<KeyedService> AndroidProfileBrowserCollectionServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<AndroidProfileBrowserCollectionService>(
      Profile::FromBrowserContext(context));
}

content::BrowserContext*
AndroidProfileBrowserCollectionServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Only instantiate the service for Profiles that support Browser instances.
  if (!Profile::FromBrowserContext(context)->AllowsBrowserWindows()) {
    return nullptr;
  }
  return context;
}
