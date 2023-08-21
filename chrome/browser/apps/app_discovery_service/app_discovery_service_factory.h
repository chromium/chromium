// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace apps {

class AppDiscoveryService;

// Singleton that owns all AppDiscoveryService instances and associates them
// with Profile.
class AppDiscoveryServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AppDiscoveryService* GetForProfile(Profile* profile);
  static AppDiscoveryServiceFactory* GetInstance();

  AppDiscoveryServiceFactory(const AppDiscoveryServiceFactory&) = delete;
  AppDiscoveryServiceFactory& operator=(const AppDiscoveryServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<AppDiscoveryServiceFactory>;

  AppDiscoveryServiceFactory();
  ~AppDiscoveryServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_SERVICE_FACTORY_H_
