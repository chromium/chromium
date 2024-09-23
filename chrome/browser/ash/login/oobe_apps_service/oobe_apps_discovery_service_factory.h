// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_APPS_SERVICE_OOBE_APPS_DISCOVERY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_APPS_SERVICE_OOBE_APPS_DISCOVERY_SERVICE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_discovery_service.h"
#include "chrome/browser/ash/login/oobe_apps_service/proto/oobe.pb.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {

// Singleton that owns all OobeAppsDiscoveryService instances and associates
// them with Profile.
class OobeAppsDiscoveryServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static OobeAppsDiscoveryService* GetForProfile(Profile* profile);
  static OobeAppsDiscoveryServiceFactory* GetInstance();

  OobeAppsDiscoveryServiceFactory(const OobeAppsDiscoveryServiceFactory&) =
      delete;
  OobeAppsDiscoveryServiceFactory& operator=(
      const OobeAppsDiscoveryServiceFactory&) = delete;

  void SetOobeAppsDiscoveryServiceForTesting(
      OobeAppsDiscoveryService* oobe_apps_dicovery_service_for_testing);

 private:
  friend base::NoDestructor<OobeAppsDiscoveryServiceFactory>;

  OobeAppsDiscoveryServiceFactory();
  ~OobeAppsDiscoveryServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  raw_ptr<OobeAppsDiscoveryService> oobe_apps_dicovery_service_for_testing_ =
      nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_APPS_SERVICE_OOBE_APPS_DISCOVERY_SERVICE_FACTORY_H_
