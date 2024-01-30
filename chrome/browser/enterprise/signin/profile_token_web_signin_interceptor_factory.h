// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_PROFILE_TOKEN_WEB_SIGNIN_INTERCEPTOR_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_PROFILE_TOKEN_WEB_SIGNIN_INTERCEPTOR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ProfileTokenWebSigninInterceptor;
class Profile;

class ProfileTokenWebSigninInterceptorFactory
    : public ProfileKeyedServiceFactory {
 public:
  static ProfileTokenWebSigninInterceptor* GetForProfile(Profile* profile);
  static ProfileTokenWebSigninInterceptorFactory* GetInstance();

  ProfileTokenWebSigninInterceptorFactory(
      const ProfileTokenWebSigninInterceptorFactory&) = delete;
  ProfileTokenWebSigninInterceptorFactory& operator=(
      const ProfileTokenWebSigninInterceptorFactory&) = delete;

 private:
  friend class base::NoDestructor<ProfileTokenWebSigninInterceptorFactory>;
  ProfileTokenWebSigninInterceptorFactory();
  ~ProfileTokenWebSigninInterceptorFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_PROFILE_TOKEN_WEB_SIGNIN_INTERCEPTOR_FACTORY_H_
