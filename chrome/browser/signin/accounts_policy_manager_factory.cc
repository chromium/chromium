// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/accounts_policy_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"

// static
AccountsPolicyManagerFactory* AccountsPolicyManagerFactory::GetInstance() {
  static base::NoDestructor<AccountsPolicyManagerFactory> instance;
  return instance.get();
}

// static
AccountsPolicyManager* AccountsPolicyManagerFactory::GetForProfile(
    Profile* profile) {
  DCHECK(profile);
  return static_cast<AccountsPolicyManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

AccountsPolicyManagerFactory::AccountsPolicyManagerFactory()
    : ProfileKeyedServiceFactory(
          "AccountsPolicyManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
}

AccountsPolicyManagerFactory::~AccountsPolicyManagerFactory() = default;

std::unique_ptr<KeyedService>
AccountsPolicyManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<AccountsPolicyManager>(profile);
}
