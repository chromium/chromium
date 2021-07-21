// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_APPS_FETCHER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_APPS_FETCHER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

class AppsFetcherService;

// Singleton that owns all AppsFetcherService instances and associates them with
// Profile.
class AppsFetcherServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppsFetcherService* GetForProfile(Profile* profile);
  static AppsFetcherServiceFactory* GetInstance();

  AppsFetcherServiceFactory(const AppsFetcherServiceFactory&) = delete;
  AppsFetcherServiceFactory& operator=(const AppsFetcherServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<AppsFetcherServiceFactory>;

  AppsFetcherServiceFactory();
  ~AppsFetcherServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_APPS_FETCHER_SERVICE_FACTORY_H_
