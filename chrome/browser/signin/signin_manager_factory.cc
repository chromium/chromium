// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"

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
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  PrefService* prefs = profile->GetPrefs();
  if (!signin::IsImplicitBrowserSigninOrExplicitDisabled(identity_manager,
                                                         prefs)) {
    // The `SigninManager` isn't needed to update the primary account as it is
    // set/cleared only on explicit user action (e.g. Sign in/Sign out from
    // chrome UI).
    return nullptr;
  }

  return std::make_unique<SigninManager>(
      *prefs, *identity_manager,
      *ChromeSigninClientFactory::GetForProfile(profile));
}

bool SigninManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool SigninManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
