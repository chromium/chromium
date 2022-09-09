// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_AUTH_ERROR_OBSERVER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_AUTH_ERROR_OBSERVER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {
class AuthErrorObserver;

// Singleton that owns all AuthErrorObserver and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated AuthErrorObserver.
class AuthErrorObserverFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of AuthErrorObserver associated with this
  // `profile` (creates one if none exists).
  static AuthErrorObserver* GetForProfile(Profile* profile);

  // Returns an instance of the AuthErrorObserverFactory singleton.
  static AuthErrorObserverFactory* GetInstance();

  AuthErrorObserverFactory(const AuthErrorObserverFactory&) = delete;
  AuthErrorObserverFactory& operator=(const AuthErrorObserverFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<AuthErrorObserverFactory>;

  AuthErrorObserverFactory();
  ~AuthErrorObserverFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_AUTH_ERROR_OBSERVER_FACTORY_H_
