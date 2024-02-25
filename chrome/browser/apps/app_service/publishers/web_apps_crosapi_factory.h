// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_CROSAPI_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_CROSAPI_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace apps {

class WebAppsCrosapi;

// Singleton that owns all WebAppsCrosapi publisher and associates them with
// Profiles.
class WebAppsCrosapiFactory : public ProfileKeyedServiceFactory {
 public:
  static WebAppsCrosapi* GetForProfile(Profile* profile);

  static WebAppsCrosapiFactory* GetInstance();

  static void ShutDownForTesting(content::BrowserContext* context);

 private:
  friend base::NoDestructor<WebAppsCrosapiFactory>;

  WebAppsCrosapiFactory();
  WebAppsCrosapiFactory(const WebAppsCrosapiFactory&) = delete;
  WebAppsCrosapiFactory& operator=(const WebAppsCrosapiFactory&) = delete;
  ~WebAppsCrosapiFactory() override = default;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_CROSAPI_FACTORY_H_
