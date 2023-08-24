// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace ash {

class SystemWebAppManager;

// Singleton factory that creates all SystemWebAppManagers and associates them
// with Profile. Clients of SystemWebAppManager shouldn't use this class to
// obtain SystemWebAppManager instances, instead they should call
// SystemWebAppManager static methods.
class SystemWebAppManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  SystemWebAppManagerFactory(const SystemWebAppManagerFactory&) = delete;
  SystemWebAppManagerFactory& operator=(const SystemWebAppManagerFactory&) =
      delete;

  static SystemWebAppManagerFactory* GetInstance();

  static bool IsServiceCreatedForProfile(Profile* profile);

 private:
  friend base::NoDestructor<SystemWebAppManagerFactory>;
  friend class SystemWebAppManager;

  SystemWebAppManagerFactory();
  ~SystemWebAppManagerFactory() override;

  // Called by SystemWebAppManager static methods.
  static SystemWebAppManager* GetForProfile(Profile* profile);

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_FACTORY_H_
