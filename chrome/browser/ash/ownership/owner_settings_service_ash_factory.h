// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_OWNERSHIP_OWNER_SETTINGS_SERVICE_ASH_FACTORY_H_
#define CHROME_BROWSER_ASH_OWNERSHIP_OWNER_SETTINGS_SERVICE_ASH_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}  // namespace content

namespace ownership {
class OwnerKeyUtil;
}  // namespace ownership

namespace ash {

class DeviceSettingsService;
class OwnerSettingsServiceAsh;
class StubCrosSettingsProvider;

class OwnerSettingsServiceAshFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static OwnerSettingsServiceAsh* GetForBrowserContext(
      content::BrowserContext* context);

  static OwnerSettingsServiceAshFactory* GetInstance();

  static void SetDeviceSettingsServiceForTesting(
      DeviceSettingsService* device_settings_service);

  static void SetStubCrosSettingsProviderForTesting(
      StubCrosSettingsProvider* stub_cros_settings_provider);

  scoped_refptr<ownership::OwnerKeyUtil> GetOwnerKeyUtil();

  void SetOwnerKeyUtilForTesting(
      const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util);

 private:
  friend struct base::DefaultSingletonTraits<OwnerSettingsServiceAshFactory>;

  OwnerSettingsServiceAshFactory();
  ~OwnerSettingsServiceAshFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;

  scoped_refptr<ownership::OwnerKeyUtil> owner_key_util_;

  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceAshFactory);
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::OwnerSettingsServiceAshFactory;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_OWNERSHIP_OWNER_SETTINGS_SERVICE_ASH_FACTORY_H_
