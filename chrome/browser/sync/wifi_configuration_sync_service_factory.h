// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_WIFI_CONFIGURATION_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_WIFI_CONFIGURATION_SYNC_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash::sync_wifi {
class WifiConfigurationSyncService;
}

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class WifiConfigurationSyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ash::sync_wifi::WifiConfigurationSyncService* GetForProfile(
      Profile* profile,
      bool create);
  static WifiConfigurationSyncServiceFactory* GetInstance();

  WifiConfigurationSyncServiceFactory(
      const WifiConfigurationSyncServiceFactory&) = delete;
  WifiConfigurationSyncServiceFactory& operator=(
      const WifiConfigurationSyncServiceFactory&) = delete;

  static bool ShouldRunInProfile(const Profile* profile);

 private:
  friend base::NoDestructor<WifiConfigurationSyncServiceFactory>;

  WifiConfigurationSyncServiceFactory();
  ~WifiConfigurationSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // CHROME_BROWSER_SYNC_WIFI_CONFIGURATION_SYNC_SERVICE_FACTORY_H_
