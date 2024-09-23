// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service_factory.h"

#include <string>

#include "base/no_destructor.h"
#include "chrome/browser/offline_pages/android/auto_fetch_notifier.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"

namespace offline_pages {

class OfflinePageAutoFetcherServiceFactory::ServiceDelegate final
    : public OfflinePageAutoFetcherService::Delegate {
  void ShowAutoFetchCompleteNotification(const std::u16string& pageTitle,
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
  static base::NoDestructor<OfflinePageAutoFetcherServiceFactory> instance;
  return instance.get();
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
    : ProfileKeyedServiceFactory(
          "OfflinePageAutoFetcherService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()),
      service_delegate_(
          std::make_unique<
              OfflinePageAutoFetcherServiceFactory::ServiceDelegate>()) {
  DependsOn(RequestCoordinatorFactory::GetInstance());
  // Depends on OfflinePageModelFactory in SimpleDependencyManager.
}

OfflinePageAutoFetcherServiceFactory::~OfflinePageAutoFetcherServiceFactory() =
    default;

std::unique_ptr<KeyedService>
OfflinePageAutoFetcherServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(context);
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(context);
  return std::make_unique<OfflinePageAutoFetcherService>(
      coordinator, model, service_delegate_.get());
}

}  // namespace offline_pages
