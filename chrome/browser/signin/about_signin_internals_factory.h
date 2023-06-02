// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ABOUT_SIGNIN_INTERNALS_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_ABOUT_SIGNIN_INTERNALS_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class AboutSigninInternals;
class Profile;

// Singleton that owns all AboutSigninInternals and associates them with
// Profiles.
class AboutSigninInternalsFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of AboutSigninInternals associated with this profile,
  // creating one if none exists.
  static AboutSigninInternals* GetForProfile(Profile* profile);

  // Returns an instance of the AboutSigninInternalsFactory singleton.
  static AboutSigninInternalsFactory* GetInstance();

  // Implementation of BrowserContextKeyedServiceFactory.
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

 private:
  friend base::NoDestructor<AboutSigninInternalsFactory>;

  AboutSigninInternalsFactory();
  ~AboutSigninInternalsFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_ABOUT_SIGNIN_INTERNALS_FACTORY_H_
