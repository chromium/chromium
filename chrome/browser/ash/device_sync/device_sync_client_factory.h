// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DEVICE_SYNC_DEVICE_SYNC_CLIENT_FACTORY_H_
#define CHROME_BROWSER_ASH_DEVICE_SYNC_DEVICE_SYNC_CLIENT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {
namespace device_sync {

class DeviceSyncClient;

// Singleton that owns all DeviceSyncClient instances and associates them with
// Profiles.
class DeviceSyncClientFactory : public ProfileKeyedServiceFactory {
 public:
  static DeviceSyncClient* GetForProfile(Profile* profile);

  static DeviceSyncClientFactory* GetInstance();

  DeviceSyncClientFactory(const DeviceSyncClientFactory&) = delete;
  DeviceSyncClientFactory& operator=(const DeviceSyncClientFactory&) = delete;

 private:
  friend base::NoDestructor<DeviceSyncClientFactory>;

  DeviceSyncClientFactory();
  ~DeviceSyncClientFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace device_sync
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DEVICE_SYNC_DEVICE_SYNC_CLIENT_FACTORY_H_
