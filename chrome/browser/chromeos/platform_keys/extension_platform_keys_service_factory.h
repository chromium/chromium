// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_EXTENSION_PLATFORM_KEYS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_EXTENSION_PLATFORM_KEYS_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace chromeos {

class ExtensionPlatformKeysService;

// Factory to create ExtensionPlatformKeysService.
class ExtensionPlatformKeysServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ExtensionPlatformKeysService* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionPlatformKeysServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ExtensionPlatformKeysServiceFactory>;

  ExtensionPlatformKeysServiceFactory();
  ExtensionPlatformKeysServiceFactory(
      const ExtensionPlatformKeysServiceFactory&) = delete;
  auto operator=(const ExtensionPlatformKeysServiceFactory&) = delete;
  ~ExtensionPlatformKeysServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_EXTENSION_PLATFORM_KEYS_SERVICE_FACTORY_H_
