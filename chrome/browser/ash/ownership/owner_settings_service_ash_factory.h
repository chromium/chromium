// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_OWNERSHIP_OWNER_SETTINGS_SERVICE_ASH_FACTORY_H_
#define CHROME_BROWSER_ASH_OWNERSHIP_OWNER_SETTINGS_SERVICE_ASH_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

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

class OwnerSettingsServiceAshFactory : public ProfileKeyedServiceFactory {
 public:
  static OwnerSettingsServiceAsh* GetForBrowserContext(
      content::BrowserContext* context);

  static OwnerSettingsServiceAshFactory* GetInstance();

  OwnerSettingsServiceAshFactory(const OwnerSettingsServiceAshFactory&) =
      delete;
  OwnerSettingsServiceAshFactory& operator=(
      const OwnerSettingsServiceAshFactory&) = delete;

  static void SetDeviceSettingsServiceForTesting(
      DeviceSettingsService* device_settings_service);

  static void SetStubCrosSettingsProviderForTesting(
      StubCrosSettingsProvider* stub_cros_settings_provider);

  scoped_refptr<ownership::OwnerKeyUtil> GetOwnerKeyUtil();

  void SetOwnerKeyUtilForTesting(
      const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util);

 private:
  friend base::NoDestructor<OwnerSettingsServiceAshFactory>;

  OwnerSettingsServiceAshFactory();
  ~OwnerSettingsServiceAshFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;

  scoped_refptr<ownership::OwnerKeyUtil> owner_key_util_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_OWNERSHIP_OWNER_SETTINGS_SERVICE_ASH_FACTORY_H_
