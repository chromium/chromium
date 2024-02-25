// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_SIGNIN_ERROR_NOTIFIER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_SIGNIN_ERROR_NOTIFIER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {
class SigninErrorNotifier;

// Singleton that owns all SigninErrorNotifiers and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated SigninErrorNotifier.
class SigninErrorNotifierFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of SigninErrorNotifier associated with this
  // profile, creating one if none exists and the shell exists.
  static SigninErrorNotifier* GetForProfile(Profile* profile);

  // Returns an instance of the SigninErrorNotifierFactory singleton.
  static SigninErrorNotifierFactory* GetInstance();

  SigninErrorNotifierFactory(const SigninErrorNotifierFactory&) = delete;
  SigninErrorNotifierFactory& operator=(const SigninErrorNotifierFactory&) =
      delete;

 private:
  friend base::NoDestructor<SigninErrorNotifierFactory>;

  SigninErrorNotifierFactory();
  ~SigninErrorNotifierFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_SIGNIN_ERROR_NOTIFIER_FACTORY_H_
