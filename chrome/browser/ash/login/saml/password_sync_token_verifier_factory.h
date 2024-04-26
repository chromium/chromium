// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_VERIFIER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_VERIFIER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace ash {
class PasswordSyncTokenVerifier;

// Singleton that owns all PasswordSyncTokenVerifiers and associates them
// with Profiles.
class PasswordSyncTokenVerifierFactory : public ProfileKeyedServiceFactory {
 public:
  static PasswordSyncTokenVerifierFactory* GetInstance();

  static PasswordSyncTokenVerifier* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<PasswordSyncTokenVerifierFactory>;

  PasswordSyncTokenVerifierFactory();
  ~PasswordSyncTokenVerifierFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_VERIFIER_FACTORY_H_
