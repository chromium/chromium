// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APPS_APK_WEB_APP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_APPS_APK_WEB_APP_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

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
class ApkWebAppServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ApkWebAppService* GetForProfile(Profile* profile);

  static ApkWebAppServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ApkWebAppServiceFactory>;

  ApkWebAppServiceFactory();
  ~ApkWebAppServiceFactory() override;

  // KeyedServiceBaseFactory:
  bool ServiceIsNULLWhileTesting() const override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ApkWebAppServiceFactory);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APPS_APK_WEB_APP_SERVICE_FACTORY_H_
