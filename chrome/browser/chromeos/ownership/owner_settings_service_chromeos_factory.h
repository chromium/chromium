// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_CHROMEOS_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_CHROMEOS_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}

namespace ownership {
class OwnerKeyUtil;
}

namespace chromeos {

class DeviceSettingsService;
class OwnerSettingsServiceChromeOS;
class StubCrosSettingsProvider;

class OwnerSettingsServiceChromeOSFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static OwnerSettingsServiceChromeOS* GetForBrowserContext(
      content::BrowserContext* context);

  static OwnerSettingsServiceChromeOSFactory* GetInstance();

  static void SetDeviceSettingsServiceForTesting(
      DeviceSettingsService* device_settings_service);

  static void SetStubCrosSettingsProviderForTesting(
      StubCrosSettingsProvider* stub_cros_settings_provider);

  scoped_refptr<ownership::OwnerKeyUtil> GetOwnerKeyUtil();

  void SetOwnerKeyUtilForTesting(
      const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util);

 private:
  friend struct base::DefaultSingletonTraits<
      OwnerSettingsServiceChromeOSFactory>;

  OwnerSettingsServiceChromeOSFactory();
  ~OwnerSettingsServiceChromeOSFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;

  scoped_refptr<ownership::OwnerKeyUtil> owner_key_util_;

  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceChromeOSFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_CHROMEOS_FACTORY_H_
