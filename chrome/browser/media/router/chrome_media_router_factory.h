// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_CHROME_MEDIA_ROUTER_FACTORY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_CHROME_MEDIA_ROUTER_FACTORY_H_

#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "components/media_router/browser/media_router_factory.h"

namespace content {
class BrowserContext;
}

namespace media_router {

// A version of MediaRouterFactory for Chrome, which refers incongito contexts
// to their parent Profile. It also adds support for desktop features.
class ChromeMediaRouterFactory : public MediaRouterFactory {
 public:
  ChromeMediaRouterFactory(const ChromeMediaRouterFactory&) = delete;
  ChromeMediaRouterFactory& operator=(const ChromeMediaRouterFactory&) = delete;

  static ChromeMediaRouterFactory* GetInstance();

  // Performs platform and Chrome-specific initialization for media_router.
  static void DoPlatformInit();

 private:
  friend struct base::LazyInstanceTraitsBase<ChromeMediaRouterFactory>;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterFactoryTest,
                           IncognitoBrowserContextShutdown);

  ChromeMediaRouterFactory();
  ~ChromeMediaRouterFactory() override;

  // BrowserContextKeyedServiceFactory interface.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void BrowserContextShutdown(content::BrowserContext* context) override;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_CHROME_MEDIA_ROUTER_FACTORY_H_
