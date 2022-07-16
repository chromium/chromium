// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class SigninGlobalError;
class Profile;

// Singleton that owns all SigninGlobalErrors and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated SigninGlobalError.
class SigninGlobalErrorFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of SigninGlobalError associated with this
  // profile, creating one if none exists. In Ash, this will return NULL.
  static SigninGlobalError* GetForProfile(Profile* profile);

  // Returns an instance of the SigninGlobalErrorFactory singleton.
  static SigninGlobalErrorFactory* GetInstance();

  SigninGlobalErrorFactory(const SigninGlobalErrorFactory&) = delete;
  SigninGlobalErrorFactory& operator=(const SigninGlobalErrorFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<SigninGlobalErrorFactory>;

  SigninGlobalErrorFactory();
  ~SigninGlobalErrorFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_FACTORY_H_
