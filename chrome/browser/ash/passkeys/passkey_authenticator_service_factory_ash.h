// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PASSKEYS_PASSKEY_AUTHENTICATOR_SERVICE_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_PASSKEYS_PASSKEY_AUTHENTICATOR_SERVICE_FACTORY_ASH_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace ash {

class PasskeyAuthenticatorServiceAsh;

class PasskeyAuthenticatorServiceFactoryAsh
    : public ProfileKeyedServiceFactory {
 public:
  static PasskeyAuthenticatorServiceFactoryAsh* GetInstance();
  static PasskeyAuthenticatorServiceAsh* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<PasskeyAuthenticatorServiceFactoryAsh>;

  PasskeyAuthenticatorServiceFactoryAsh();
  ~PasskeyAuthenticatorServiceFactoryAsh() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PASSKEYS_PASSKEY_AUTHENTICATOR_SERVICE_FACTORY_ASH_H_
