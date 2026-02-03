// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_KEYSTORE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_KEYSTORE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace ash {

class KeystoreService;

class KeystoreServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static KeystoreService* GetForBrowserContext(
      content::BrowserContext* context);
  static KeystoreServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<KeystoreServiceFactory>;

  KeystoreServiceFactory();
  ~KeystoreServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_KEYSTORE_SERVICE_FACTORY_H_
