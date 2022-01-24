// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_FACTORY_H_

#include "base/memory/singleton.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

// Singleton that owns all AppServiceProxy's and associates them with Profile.
class AppServiceProxyFactory : public BrowserContextKeyedServiceFactory {
 public:
  static bool IsAppServiceAvailableForProfile(Profile* profile);

  static AppServiceProxy* GetForProfile(Profile* profile);

  static AppServiceProxyFactory* GetInstance();

  AppServiceProxyFactory(const AppServiceProxyFactory&) = delete;
  AppServiceProxyFactory& operator=(const AppServiceProxyFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<AppServiceProxyFactory>;

  AppServiceProxyFactory();
  ~AppServiceProxyFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_FACTORY_H_
