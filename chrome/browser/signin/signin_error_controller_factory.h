// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_ERROR_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_ERROR_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/signin/core/browser/signin_error_controller.h"

// Singleton that owns all SigninErrorControllers and associates them with
// Profiles.
class SigninErrorControllerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of SigninErrorController associated with this profile
  // (creating one if none exists). Returns NULL if this profile cannot have an
  // SigninClient (for example, if |profile| is incognito).
  static SigninErrorController* GetForProfile(Profile* profile);

  // Returns an instance of the factory singleton.
  static SigninErrorControllerFactory* GetInstance();

 private:
  friend base::NoDestructor<SigninErrorControllerFactory>;

  SigninErrorControllerFactory();
  ~SigninErrorControllerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_ERROR_CONTROLLER_FACTORY_H_
