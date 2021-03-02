// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_SIGNIN_ERROR_NOTIFIER_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_SIGNIN_ERROR_NOTIFIER_FACTORY_ASH_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class SigninErrorNotifier;
class Profile;

// Singleton that owns all SigninErrorNotifiers and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated SigninErrorNotifier.
class SigninErrorNotifierFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of SigninErrorNotifier associated with this
  // profile, creating one if none exists and the shell exists.
  static SigninErrorNotifier* GetForProfile(Profile* profile);

  // Returns an instance of the SigninErrorNotifierFactory singleton.
  static SigninErrorNotifierFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<SigninErrorNotifierFactory>;

  SigninErrorNotifierFactory();
  ~SigninErrorNotifierFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(SigninErrorNotifierFactory);
};

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_SIGNIN_ERROR_NOTIFIER_FACTORY_ASH_H_
