// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CACHED_IMAGE_FETCHER_CACHED_IMAGE_FETCHER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CACHED_IMAGE_FETCHER_CACHED_IMAGE_FETCHER_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace image_fetcher {

class CachedImageFetcherService;

// Factory to create one CachedImageFetcherService per browser context.
class CachedImageFetcherServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static CachedImageFetcherService* GetForBrowserContext(
      content::BrowserContext* context);
  static CachedImageFetcherServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<CachedImageFetcherServiceFactory>;

  CachedImageFetcherServiceFactory();
  ~CachedImageFetcherServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcherServiceFactory);
};

}  // namespace image_fetcher

#endif  // CHROME_BROWSER_CACHED_IMAGE_FETCHER_CACHED_IMAGE_FETCHER_SERVICE_FACTORY_H_
