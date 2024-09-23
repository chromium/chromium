// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_AUTHENTICATION_SIGNIN_INTERCEPTOR_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_AUTHENTICATION_SIGNIN_INTERCEPTOR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class OidcAuthenticationSigninInterceptor;
class Profile;

class OidcAuthenticationSigninInterceptorFactory
    : public ProfileKeyedServiceFactory {
 public:
  static OidcAuthenticationSigninInterceptor* GetForProfile(Profile* profile);
  static OidcAuthenticationSigninInterceptorFactory* GetInstance();

  OidcAuthenticationSigninInterceptorFactory(
      const OidcAuthenticationSigninInterceptorFactory&) = delete;
  OidcAuthenticationSigninInterceptorFactory& operator=(
      const OidcAuthenticationSigninInterceptorFactory&) = delete;

 private:
  friend class base::NoDestructor<OidcAuthenticationSigninInterceptorFactory>;
  OidcAuthenticationSigninInterceptorFactory();
  ~OidcAuthenticationSigninInterceptorFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_AUTHENTICATION_SIGNIN_INTERCEPTOR_FACTORY_H_
