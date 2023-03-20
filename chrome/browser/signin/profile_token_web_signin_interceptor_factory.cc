// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_token_web_signin_interceptor_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_token_web_signin_interceptor.h"

// static
ProfileTokenWebSigninInterceptor*
ProfileTokenWebSigninInterceptorFactory::GetForProfile(Profile* profile) {
  return static_cast<ProfileTokenWebSigninInterceptor*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

//  static
ProfileTokenWebSigninInterceptorFactory*
ProfileTokenWebSigninInterceptorFactory::GetInstance() {
  static base::NoDestructor<ProfileTokenWebSigninInterceptorFactory> instance;
  return instance.get();
}

ProfileTokenWebSigninInterceptorFactory::
    ProfileTokenWebSigninInterceptorFactory()
    : ProfileKeyedServiceFactory("ProfileTokenWebSigninInterceptor") {}

ProfileTokenWebSigninInterceptorFactory::
    ~ProfileTokenWebSigninInterceptorFactory() = default;

KeyedService* ProfileTokenWebSigninInterceptorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ProfileTokenWebSigninInterceptor(
      Profile::FromBrowserContext(context));
}
