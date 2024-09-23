// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor_factory.h"

#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor.h"
#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"

// static
OidcAuthenticationSigninInterceptor*
OidcAuthenticationSigninInterceptorFactory::GetForProfile(Profile* profile) {
  return (base::FeatureList::IsEnabled(
             profile_management::features::kOidcAuthProfileManagement))
             ? static_cast<OidcAuthenticationSigninInterceptor*>(
                   GetInstance()->GetServiceForBrowserContext(profile, true))
             : nullptr;
}

//  static
OidcAuthenticationSigninInterceptorFactory*
OidcAuthenticationSigninInterceptorFactory::GetInstance() {
  static base::NoDestructor<OidcAuthenticationSigninInterceptorFactory>
      instance;
  return instance.get();
}

OidcAuthenticationSigninInterceptorFactory::
    OidcAuthenticationSigninInterceptorFactory()
    : ProfileKeyedServiceFactory("OidcAuthenticationSigninInterceptor") {
  DependsOn(enterprise::ProfileIdServiceFactory::GetInstance());
  DependsOn(policy::UserPolicySigninServiceFactory::GetInstance());
  DependsOn(policy::UserPolicyOidcSigninServiceFactory::GetInstance());
}

OidcAuthenticationSigninInterceptorFactory::
    ~OidcAuthenticationSigninInterceptorFactory() = default;

std::unique_ptr<KeyedService> OidcAuthenticationSigninInterceptorFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<OidcAuthenticationSigninInterceptor>(
      Profile::FromBrowserContext(context),
      std::make_unique<DiceWebSigninInterceptorDelegate>());
}
