// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_LOGIN_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_LOGIN_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {

class OAuth2LoginManager;

// Singleton that owns all OAuth2LoginManager and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated OAuth2LoginManager.
class OAuth2LoginManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of OAuth2LoginManager associated with this
  // `profile` (creates one if none exists).
  static OAuth2LoginManager* GetForProfile(Profile* profile);

  // Returns an instance of the OAuth2LoginManagerFactory singleton.
  static OAuth2LoginManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<OAuth2LoginManagerFactory>;

  OAuth2LoginManagerFactory();
  ~OAuth2LoginManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(OAuth2LoginManagerFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_LOGIN_MANAGER_FACTORY_H_
