// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROFILE_ATTRIBUTES_UPDATER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROFILE_ATTRIBUTES_UPDATER_FACTORY_H_

#include "base/memory/singleton.h"
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
  friend struct base::DefaultSingletonTraits<
      SigninProfileAttributesUpdaterFactory>;

  SigninProfileAttributesUpdaterFactory();
  ~SigninProfileAttributesUpdaterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROFILE_ATTRIBUTES_UPDATER_FACTORY_H_
