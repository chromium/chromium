// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_LACROS_WEB_APPS_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_LACROS_WEB_APPS_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

class LacrosWebApps;

// Singleton that owns all LacrosWebApps publisher and associates them with
// Profiles.
class LacrosWebAppsFactory : public BrowserContextKeyedServiceFactory {
 public:
  static LacrosWebApps* GetForProfile(Profile* profile);

  static LacrosWebAppsFactory* GetInstance();

  static void ShutDownForTesting(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<LacrosWebAppsFactory>;

  LacrosWebAppsFactory();
  LacrosWebAppsFactory(const LacrosWebAppsFactory&) = delete;
  LacrosWebAppsFactory& operator=(const LacrosWebAppsFactory&) = delete;
  ~LacrosWebAppsFactory() override = default;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_LACROS_WEB_APPS_FACTORY_H_
