// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"

class SigninManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns an instance of the factory singleton.
  static SigninManagerFactory* GetInstance();

  static SigninManager* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<SigninManagerFactory>;

  SigninManagerFactory();
  ~SigninManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FACTORY_H_
