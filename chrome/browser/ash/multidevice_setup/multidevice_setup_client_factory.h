// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_CLIENT_FACTORY_H_
#define CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_CLIENT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ChromeOSSystemProfileProviderTest;
class ChromeMetricsServiceClientTestIgnoredForAppMetrics;
class ChromeMetricsServiceClientTest;
class Profile;

namespace ash {
namespace multidevice_setup {

class MultiDeviceSetupClient;

// Singleton that owns all MultiDeviceSetupClient instances and associates them
// with Profiles.
class MultiDeviceSetupClientFactory : public ProfileKeyedServiceFactory {
 public:
  static MultiDeviceSetupClient* GetForProfile(Profile* profile);

  static MultiDeviceSetupClientFactory* GetInstance();

  MultiDeviceSetupClientFactory(const MultiDeviceSetupClientFactory&) = delete;
  MultiDeviceSetupClientFactory& operator=(
      const MultiDeviceSetupClientFactory&) = delete;

 private:
  friend base::NoDestructor<MultiDeviceSetupClientFactory>;
  friend class ::ChromeOSSystemProfileProviderTest;
  friend class ::ChromeMetricsServiceClientTestIgnoredForAppMetrics;
  friend class ::ChromeMetricsServiceClientTest;

  MultiDeviceSetupClientFactory();
  ~MultiDeviceSetupClientFactory() override;

  void SetServiceIsNULLWhileTestingForTesting(
      bool service_is_null_while_testing) {
    service_is_null_while_testing_ = service_is_null_while_testing;
  }

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool service_is_null_while_testing_ = true;
};

}  // namespace multidevice_setup
}  // namespace ash

namespace chromeos {
namespace multidevice_setup {
using ::ash::multidevice_setup::MultiDeviceSetupClientFactory;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_CLIENT_FACTORY_H_
