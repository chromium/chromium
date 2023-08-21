// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APPS_APK_WEB_APP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_APPS_APK_WEB_APP_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace ash {

class ApkWebAppService;

// Singleton that owns all ApkWebAppServices and associates them with Profiles.
// Listens for the Profile's destruction notification and cleans up the
// associated ApkWebAppService.
//
// ApkWebAppService may be created for any profile that supports ARC.
class ApkWebAppServiceFactory : public ProfileKeyedServiceFactory {
 public:
  ApkWebAppServiceFactory(const ApkWebAppServiceFactory&) = delete;
  ApkWebAppServiceFactory& operator=(const ApkWebAppServiceFactory&) = delete;

  static ApkWebAppService* GetForProfile(Profile* profile);

  static ApkWebAppServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ApkWebAppServiceFactory>;

  ApkWebAppServiceFactory();
  ~ApkWebAppServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APPS_APK_WEB_APP_SERVICE_FACTORY_H_
