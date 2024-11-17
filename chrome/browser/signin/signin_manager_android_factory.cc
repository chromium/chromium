// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_android_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/android/signin_manager_android.h"
#include "chrome/browser/signin/identity_manager_factory.h"

SigninManagerAndroidFactory::SigninManagerAndroidFactory()
    : ProfileKeyedServiceFactory(
          "SigninManagerAndroid",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
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

std::unique_ptr<KeyedService>
SigninManagerAndroidFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  return std::make_unique<SigninManagerAndroid>(profile, identity_manager);
}
