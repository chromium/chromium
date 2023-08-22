// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_KEYSTORE_SERVICE_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_KEYSTORE_SERVICE_FACTORY_ASH_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace crosapi {

class KeystoreServiceAsh;

// The factory to produce additional KeystoreService-s when they are needed
// (most notably, for multi-sign-in feature). Most of the time it is expected to
// return the precreated KestoreService from CrosapiManager. When multi-sign-in
// feature is removed, the additional KeystoreService-s and this factory
// probably won't be needed anymore.
class KeystoreServiceFactoryAsh : public ProfileKeyedServiceFactory {
 public:
  static KeystoreServiceAsh* GetForBrowserContext(
      content::BrowserContext* context);
  static KeystoreServiceFactoryAsh* GetInstance();

 private:
  friend class base::NoDestructor<KeystoreServiceFactoryAsh>;

  KeystoreServiceFactoryAsh();
  ~KeystoreServiceFactoryAsh() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_KEYSTORE_SERVICE_FACTORY_ASH_H_
