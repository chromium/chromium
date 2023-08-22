// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace ash::personalization_app {

class PersonalizationAppManager;

class PersonalizationAppManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static PersonalizationAppManager* GetForBrowserContext(
      content::BrowserContext* context);
  static PersonalizationAppManagerFactory* GetInstance();

  PersonalizationAppManagerFactory(const PersonalizationAppManagerFactory&) =
      delete;
  PersonalizationAppManagerFactory& operator=(
      const PersonalizationAppManagerFactory&) = delete;

 private:
  friend base::NoDestructor<PersonalizationAppManagerFactory>;

  PersonalizationAppManagerFactory();
  ~PersonalizationAppManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_MANAGER_FACTORY_H_
