// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUS_ADDRESSES_PLUS_ADDRESS_SETTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PLUS_ADDRESSES_PLUS_ADDRESS_SETTING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace plus_addresses {
class PlusAddressSettingService;
}

// Factory responsible for creating `PlusAddressSettingService`, which is
// responsible for managing settings synced via `syncer::PLUS_ADDRESS_SETTING`.
class PlusAddressSettingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PlusAddressSettingServiceFactory* GetInstance();
  static plus_addresses::PlusAddressSettingService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<PlusAddressSettingServiceFactory>;

  PlusAddressSettingServiceFactory();
  ~PlusAddressSettingServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PLUS_ADDRESSES_PLUS_ADDRESS_SETTING_SERVICE_FACTORY_H_
