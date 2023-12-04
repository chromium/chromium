// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"

// static
SigninManagerFactory* SigninManagerFactory::GetInstance() {
  static base::NoDestructor<SigninManagerFactory> instance;
  return instance.get();
}

// static
SigninManager* SigninManagerFactory::GetForProfile(Profile* profile) {
  DCHECK(profile);
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SigninManagerFactory::SigninManagerFactory()
    : ProfileKeyedServiceFactory("SigninManager") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
}

SigninManagerFactory::~SigninManagerFactory() = default;

std::unique_ptr<KeyedService>
SigninManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<SigninManager>(
      *profile->GetPrefs(), *IdentityManagerFactory::GetForProfile(profile),
      *ChromeSigninClientFactory::GetForProfile(profile));
}

bool SigninManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool SigninManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
