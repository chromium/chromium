// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_SYNC_DEVICE_SYNC_CLIENT_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_SYNC_DEVICE_SYNC_CLIENT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {

namespace device_sync {

class DeviceSyncClient;

// Singleton that owns all DeviceSyncClient instances and associates them with
// Profiles.
class DeviceSyncClientFactory : public BrowserContextKeyedServiceFactory {
 public:
  static DeviceSyncClient* GetForProfile(Profile* profile);

  static DeviceSyncClientFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<DeviceSyncClientFactory>;

  DeviceSyncClientFactory();
  ~DeviceSyncClientFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncClientFactory);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_SYNC_DEVICE_SYNC_CLIENT_FACTORY_H_
