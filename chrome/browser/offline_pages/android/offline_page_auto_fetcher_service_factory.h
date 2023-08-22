// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_AUTO_FETCHER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_AUTO_FETCHER_SERVICE_FACTORY_H_

#include <memory>
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace offline_pages {
class OfflinePageAutoFetcherService;

// A factory to create one unique OfflinePageAutoFetcherService.
class OfflinePageAutoFetcherServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static OfflinePageAutoFetcherServiceFactory* GetInstance();
  static OfflinePageAutoFetcherService* GetForBrowserContext(
      content::BrowserContext* context);

  OfflinePageAutoFetcherServiceFactory(
      const OfflinePageAutoFetcherServiceFactory&) = delete;
  OfflinePageAutoFetcherServiceFactory& operator=(
      const OfflinePageAutoFetcherServiceFactory&) = delete;

 private:
  class ServiceDelegate;
  friend base::NoDestructor<OfflinePageAutoFetcherServiceFactory>;

  OfflinePageAutoFetcherServiceFactory();
  ~OfflinePageAutoFetcherServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  std::unique_ptr<ServiceDelegate> service_delegate_;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_AUTO_FETCHER_SERVICE_FACTORY_H_
