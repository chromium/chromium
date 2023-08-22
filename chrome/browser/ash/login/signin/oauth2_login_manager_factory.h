// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_LOGIN_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_LOGIN_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {

class OAuth2LoginManager;

// Singleton that owns all OAuth2LoginManager and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated OAuth2LoginManager.
class OAuth2LoginManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of OAuth2LoginManager associated with this
  // `profile` (creates one if none exists).
  static OAuth2LoginManager* GetForProfile(Profile* profile);

  // Returns an instance of the OAuth2LoginManagerFactory singleton.
  static OAuth2LoginManagerFactory* GetInstance();

  OAuth2LoginManagerFactory(const OAuth2LoginManagerFactory&) = delete;
  OAuth2LoginManagerFactory& operator=(const OAuth2LoginManagerFactory&) =
      delete;

 private:
  friend base::NoDestructor<OAuth2LoginManagerFactory>;

  OAuth2LoginManagerFactory();
  ~OAuth2LoginManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_LOGIN_MANAGER_FACTORY_H_
