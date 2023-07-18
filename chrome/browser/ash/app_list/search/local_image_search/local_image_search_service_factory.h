// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_LOCAL_IMAGE_SEARCH_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_LOCAL_IMAGE_SEARCH_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace app_list {

class LocalImageSearchService;

class LocalImageSearchServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static LocalImageSearchService* GetForBrowserContext(
      content::BrowserContext* context);
  static LocalImageSearchServiceFactory* GetInstance();

  LocalImageSearchServiceFactory(const LocalImageSearchServiceFactory&) =
      delete;
  LocalImageSearchServiceFactory& operator=(
      const LocalImageSearchServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<LocalImageSearchServiceFactory>;

  LocalImageSearchServiceFactory();
  ~LocalImageSearchServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_LOCAL_IMAGE_SEARCH_SERVICE_FACTORY_H_
