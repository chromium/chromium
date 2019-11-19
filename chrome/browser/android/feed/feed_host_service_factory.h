// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_HOST_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_HOST_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace feed {

class FeedHostService;

// Factory to create one FeedHostService per browser context. Callers need to
// watch out for nullptr when incognito, as the feed should not be used then.
class FeedHostServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static FeedHostService* GetForBrowserContext(
      content::BrowserContext* context);

  static FeedHostServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<FeedHostServiceFactory>;

  FeedHostServiceFactory();
  ~FeedHostServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(FeedHostServiceFactory);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_HOST_SERVICE_FACTORY_H_
