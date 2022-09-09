// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_android_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/android/signin/signin_manager_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"

SigninManagerAndroidFactory::SigninManagerAndroidFactory()
    : ProfileKeyedServiceFactory("SigninManagerAndroid") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SigninManagerAndroidFactory::~SigninManagerAndroidFactory() {}

// static
SigninManagerAndroid* SigninManagerAndroidFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SigninManagerAndroid*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninManagerAndroidFactory* SigninManagerAndroidFactory::GetInstance() {
  static base::NoDestructor<SigninManagerAndroidFactory> instance;
  return instance.get();
}

KeyedService* SigninManagerAndroidFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  return new SigninManagerAndroid(profile, identity_manager);
}
