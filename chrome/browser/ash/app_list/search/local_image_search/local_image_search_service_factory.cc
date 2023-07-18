// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_service_factory.h"

#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace app_list {

// static
LocalImageSearchService* LocalImageSearchServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LocalImageSearchService*>(
      LocalImageSearchServiceFactory::GetInstance()
          ->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
LocalImageSearchServiceFactory* LocalImageSearchServiceFactory::GetInstance() {
  static base::NoDestructor<LocalImageSearchServiceFactory> instance;
  return instance.get();
}

LocalImageSearchServiceFactory::LocalImageSearchServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "LocalImageSearchService",
          BrowserContextDependencyManager::GetInstance()) {}

LocalImageSearchServiceFactory::~LocalImageSearchServiceFactory() = default;

std::unique_ptr<KeyedService>
LocalImageSearchServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<LocalImageSearchService>(profile);
}

bool LocalImageSearchServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // Initializes the storage at log in, so that the worker can watch files in
  // the background.
  return true;
}

}  // namespace app_list
