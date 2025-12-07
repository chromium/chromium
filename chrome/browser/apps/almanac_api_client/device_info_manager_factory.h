// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_DEVICE_INFO_MANAGER_FACTORY_H_
#define CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_DEVICE_INFO_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace apps {

class DeviceInfoManager;

// Factory for DeviceInfoManager instances, keyed by Profile.
class DeviceInfoManagerFactory : public ProfileKeyedServiceFactory {
 public:
  DeviceInfoManagerFactory(const DeviceInfoManagerFactory&) = delete;
  DeviceInfoManagerFactory& operator=(const DeviceInfoManagerFactory&) = delete;

  // Retrieve the shared DeviceInfoManager instance for the given profile.
  // Returns nullptr if the service is not supported for the profile (e.g.
  // Incognito).
  static DeviceInfoManager* GetForProfile(Profile* profile);

  static DeviceInfoManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<DeviceInfoManagerFactory>;

  DeviceInfoManagerFactory();
  ~DeviceInfoManagerFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_DEVICE_INFO_MANAGER_FACTORY_H_
