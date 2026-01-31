// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_KEYSTORE_SERVICE_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_KEYSTORE_SERVICE_FACTORY_ASH_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace crosapi {

class KeystoreServiceAsh;

class KeystoreServiceFactoryAsh : public ProfileKeyedServiceFactory {
 public:
  static KeystoreServiceAsh* GetForBrowserContext(
      content::BrowserContext* context);
  static KeystoreServiceFactoryAsh* GetInstance();

 private:
  friend class base::NoDestructor<KeystoreServiceFactoryAsh>;

  KeystoreServiceFactoryAsh();
  ~KeystoreServiceFactoryAsh() override = default;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_KEYSTORE_SERVICE_FACTORY_ASH_H_
