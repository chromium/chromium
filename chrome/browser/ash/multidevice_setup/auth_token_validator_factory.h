// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_FACTORY_H_
#define CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace ash {
namespace multidevice_setup {

class AuthTokenValidator;

// Owns AuthTokenValidator instances and associates them with Profiles.
class AuthTokenValidatorFactory : public ProfileKeyedServiceFactory {
 public:
  static AuthTokenValidator* GetForProfile(Profile* profile);

  static AuthTokenValidatorFactory* GetInstance();

  AuthTokenValidatorFactory(const AuthTokenValidatorFactory&) = delete;
  AuthTokenValidatorFactory& operator=(const AuthTokenValidatorFactory&) =
      delete;

 private:
  friend base::NoDestructor<AuthTokenValidatorFactory>;

  AuthTokenValidatorFactory();
  ~AuthTokenValidatorFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace multidevice_setup
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_FACTORY_H_
