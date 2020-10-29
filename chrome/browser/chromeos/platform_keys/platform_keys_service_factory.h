// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace chromeos {
namespace platform_keys {

class PlatformKeysService;

// Factory to create PlatformKeysService.
class PlatformKeysServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PlatformKeysService* GetForBrowserContext(
      content::BrowserContext* context);

  static PlatformKeysServiceFactory* GetInstance();

  // Returns an instance of PlatformKeysService that allows operations on the
  // device-wide key store and is not tied to a user.
  // The lifetime of the returned service is tied to the
  // PlatformKeysServiceFactory itself.
  PlatformKeysService* GetDeviceWideService();

  // When call with a nun-nullptr |device_wide_service_for_testing|, subsequent
  // calls to GetDeviceWideService() will return the passed pointer.
  // When called with nullptr, subsequent calls to GetDeviceWideService() will
  // return the default device-wide PlatformKeysService again.
  // The caller is responsible that this is called with nullptr before an object
  // previously passed in is destroyed.
  void SetDeviceWideServiceForTesting(
      PlatformKeysService* device_wide_service_for_testing);

 private:
  friend struct base::DefaultSingletonTraits<PlatformKeysServiceFactory>;

  PlatformKeysServiceFactory();
  PlatformKeysServiceFactory(const PlatformKeysServiceFactory&) = delete;
  PlatformKeysServiceFactory& operator=(const PlatformKeysServiceFactory&) =
      delete;
  ~PlatformKeysServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  PlatformKeysService* GetOrCreateDeviceWideService();

  // A PlatformKeysService that is not tied to a Profile/User and only has
  // access to the system token.
  // Initialized lazily.
  std::unique_ptr<PlatformKeysService> device_wide_service_;

  PlatformKeysService* device_wide_service_for_testing_ = nullptr;
};
}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_FACTORY_H_
