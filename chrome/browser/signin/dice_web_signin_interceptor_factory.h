// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class DiceWebSigninInterceptor;
class Profile;

class DiceWebSigninInterceptorFactory : public ProfileKeyedServiceFactory {
 public:
  static DiceWebSigninInterceptor* GetForProfile(Profile* profile);
  static DiceWebSigninInterceptorFactory* GetInstance();

  DiceWebSigninInterceptorFactory(const DiceWebSigninInterceptorFactory&) =
      delete;
  DiceWebSigninInterceptorFactory& operator=(
      const DiceWebSigninInterceptorFactory&) = delete;

 private:
  friend base::NoDestructor<DiceWebSigninInterceptorFactory>;
  DiceWebSigninInterceptorFactory();
  ~DiceWebSigninInterceptorFactory() override;

  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_FACTORY_H_
