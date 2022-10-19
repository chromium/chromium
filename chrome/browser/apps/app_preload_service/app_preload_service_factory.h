// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace apps {

class AppPreloadService;

// Singleton that owns all AppPreloadService and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated AppPreloadService.
class AppPreloadServiceFactory : public ProfileKeyedServiceFactory {
 public:
  AppPreloadServiceFactory(const AppPreloadServiceFactory&) = delete;
  AppPreloadServiceFactory& operator=(const AppPreloadServiceFactory&) = delete;

  static AppPreloadService* GetForProfile(Profile* profile);

  static AppPreloadServiceFactory* GetInstance();

  static bool IsAvailable(Profile* profile);

 private:
  friend base::NoDestructor<AppPreloadServiceFactory>;

  AppPreloadServiceFactory();
  ~AppPreloadServiceFactory() override;

  // ProfileKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVICE_FACTORY_H_
