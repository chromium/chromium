// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace ash {
namespace platform_keys {

class PlatformKeysService;

// Factory to create PlatformKeysService.
class PlatformKeysServiceFactory : public ProfileKeyedServiceFactory {
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

  // If |is_testing_mode| is true, the factory will return platform keys service
  // instances ready to be used in tests.
  // Note: Softoken NSS PKCS11 module (used for testing) allows only predefined
  // key attributes to be set and retrieved. Chaps supports setting and
  // retrieving custom attributes. If |map_to_softoken_attrs_for_testing_| is
  // true, the factory will return services that will use fake KeyAttribute
  // mappings predefined in softoken module for testing. Otherwise, the real
  // mappings to constants in
  // third_party/cros_system_api/constants/pkcs11_custom_attributes.h will be
  // used.
  void SetTestingMode(bool is_testing_mode);

 private:
  friend base::NoDestructor<PlatformKeysServiceFactory>;

  PlatformKeysServiceFactory();
  PlatformKeysServiceFactory(const PlatformKeysServiceFactory&) = delete;
  PlatformKeysServiceFactory& operator=(const PlatformKeysServiceFactory&) =
      delete;
  ~PlatformKeysServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void BrowserContextShutdown(content::BrowserContext* context) override;

  PlatformKeysService* GetOrCreateDeviceWideService();

  // A PlatformKeysService that is not tied to a Profile/User and only has
  // access to the system token.
  // Initialized lazily.
  std::unique_ptr<PlatformKeysService> device_wide_service_;

  raw_ptr<PlatformKeysService, DanglingUntriaged>
      device_wide_service_for_testing_ = nullptr;

  bool map_to_softoken_attrs_for_testing_ = false;
  bool allow_alternative_params_for_testing_ = false;
};
}  // namespace platform_keys
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_FACTORY_H_
