// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_FACTORY_H_

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

// Singleton that owns all AppServiceProxy's and associates them with Profile.
class AppServiceProxyFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns true if App Service is available for use in the given `profile`.
  // App Service is unavailable in profiles which do not run apps (e.g.
  // non-guest Incognito profiles).
  // All client code which uses AppServiceProxy should verify that App Service
  // is available before calling GetForProfile. When App Service is unavailable,
  // a common pattern is to fall back to an associated real profile (e.g. the
  // parent of the incognito profile). As this constitutes a data leak out of
  // Incognito, it is up to individual client teams to decide whether this is
  // appropriate behavior for their feature. Alternatively, feature teams can
  // disable the App Service integration in these profiles.
  static bool IsAppServiceAvailableForProfile(Profile* profile);

  // Returns the AppServiceProxy instance for the given `profile`. Should only
  // be called with a profile for which IsAppServiceAvailableForProfile returns
  // true.
  static AppServiceProxy* GetForProfile(Profile* profile);

  static AppServiceProxyFactory* GetInstance();

  AppServiceProxyFactory(const AppServiceProxyFactory&) = delete;
  AppServiceProxyFactory& operator=(const AppServiceProxyFactory&) = delete;

 private:
  friend base::NoDestructor<AppServiceProxyFactory>;

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
