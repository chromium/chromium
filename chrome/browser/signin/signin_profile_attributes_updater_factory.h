// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROFILE_ATTRIBUTES_UPDATER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROFILE_ATTRIBUTES_UPDATER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class SigninProfileAttributesUpdater;

class SigninProfileAttributesUpdaterFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Returns nullptr if this profile cannot have a
  // SigninProfileAttributesUpdater (for example, if |profile| is incognito).
  static SigninProfileAttributesUpdater* GetForProfile(Profile* profile);

  // Returns an instance of the factory singleton.
  static SigninProfileAttributesUpdaterFactory* GetInstance();

  SigninProfileAttributesUpdaterFactory(
      const SigninProfileAttributesUpdaterFactory&) = delete;
  SigninProfileAttributesUpdaterFactory& operator=(
      const SigninProfileAttributesUpdaterFactory&) = delete;

 private:
  friend base::NoDestructor<SigninProfileAttributesUpdaterFactory>;

  SigninProfileAttributesUpdaterFactory();
  ~SigninProfileAttributesUpdaterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROFILE_ATTRIBUTES_UPDATER_FACTORY_H_
