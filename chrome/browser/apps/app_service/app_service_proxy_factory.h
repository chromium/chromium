// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

class AppServiceProxy;

// Singleton that owns all AppServiceProxy's and associates them with Profile.
class AppServiceProxyFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppServiceProxy* GetForProfile(Profile* profile);

  static AppServiceProxyFactory* GetInstance();

  static bool IsEnabled();

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

  DISALLOW_COPY_AND_ASSIGN(AppServiceProxyFactory);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_FACTORY_H_
