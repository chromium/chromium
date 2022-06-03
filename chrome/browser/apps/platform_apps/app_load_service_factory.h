// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_APP_LOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_APP_LOAD_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace apps {

class AppLoadService;

class AppLoadServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppLoadService* GetForBrowserContext(content::BrowserContext* context);

  static AppLoadServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AppLoadServiceFactory>;

  AppLoadServiceFactory();
  ~AppLoadServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_APP_LOAD_SERVICE_FACTORY_H_
