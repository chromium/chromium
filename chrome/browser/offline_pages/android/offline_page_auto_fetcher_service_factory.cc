// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service_factory.h"

#include <string>

#include "base/memory/singleton.h"
#include "chrome/browser/offline_pages/android/auto_fetch_notifier.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace offline_pages {

class OfflinePageAutoFetcherServiceFactory::ServiceDelegate final
    : public OfflinePageAutoFetcherService::Delegate {
  void ShowAutoFetchCompleteNotification(const base::string16& pageTitle,
                                         const std::string& original_url,
                                         const std::string& final_url,
                                         int android_tab_id,
                                         int64_t offline_id) override {
    offline_pages::ShowAutoFetchCompleteNotification(
        pageTitle, original_url, final_url, android_tab_id, offline_id);
  }
};

// static
OfflinePageAutoFetcherServiceFactory*
OfflinePageAutoFetcherServiceFactory::GetInstance() {
  return base::Singleton<OfflinePageAutoFetcherServiceFactory>::get();
}

// static
OfflinePageAutoFetcherService*
OfflinePageAutoFetcherServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  KeyedService* service =
      GetInstance()->GetServiceForBrowserContext(context, true);
  if (!service)
    return nullptr;
  return static_cast<OfflinePageAutoFetcherService*>(service);
}

OfflinePageAutoFetcherServiceFactory::OfflinePageAutoFetcherServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflinePageAutoFetcherService",
          BrowserContextDependencyManager::GetInstance()),
      service_delegate_(
          std::make_unique<
              OfflinePageAutoFetcherServiceFactory::ServiceDelegate>()) {
  DependsOn(RequestCoordinatorFactory::GetInstance());
  // Depends on OfflinePageModelFactory in SimpleDependencyManager.
}

OfflinePageAutoFetcherServiceFactory::~OfflinePageAutoFetcherServiceFactory() {}

KeyedService* OfflinePageAutoFetcherServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(context);
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(context);
  return new OfflinePageAutoFetcherService(coordinator, model,
                                           service_delegate_.get());
}

}  // namespace offline_pages
