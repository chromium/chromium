// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

// Singleton that creates |RemoteAppsManager|s and associates them with a
// |Profile|.
class RemoteAppsManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static RemoteAppsManager* GetForProfile(Profile* profile);

  static RemoteAppsManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<RemoteAppsManagerFactory>;

  RemoteAppsManagerFactory();
  RemoteAppsManagerFactory(const RemoteAppsManagerFactory&) = delete;
  RemoteAppsManagerFactory& operator=(const RemoteAppsManagerFactory&) = delete;
  ~RemoteAppsManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_MANAGER_FACTORY_H_
