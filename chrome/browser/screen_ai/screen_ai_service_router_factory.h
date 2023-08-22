// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_ROUTER_FACTORY_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace screen_ai {

class ScreenAIServiceRouter;

// Factory to get or create an instance of ScreenAIServiceRouter for a
// BrowserContext.
class ScreenAIServiceRouterFactory : public ProfileKeyedServiceFactory {
 public:
  static screen_ai::ScreenAIServiceRouter* GetForBrowserContext(
      content::BrowserContext* context);

  static void EnsureFactoryBuilt();

 private:
  friend class base::NoDestructor<ScreenAIServiceRouterFactory>;
  static ScreenAIServiceRouterFactory* GetInstance();

  ScreenAIServiceRouterFactory();
  ~ScreenAIServiceRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_ROUTER_FACTORY_H_
