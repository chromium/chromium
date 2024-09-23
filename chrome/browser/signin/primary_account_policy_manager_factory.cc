// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/primary_account_policy_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"

// static
PrimaryAccountPolicyManagerFactory*
PrimaryAccountPolicyManagerFactory::GetInstance() {
  static base::NoDestructor<PrimaryAccountPolicyManagerFactory> instance;
  return instance.get();
}

// static
PrimaryAccountPolicyManager* PrimaryAccountPolicyManagerFactory::GetForProfile(
    Profile* profile) {
  DCHECK(profile);
  return static_cast<PrimaryAccountPolicyManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrimaryAccountPolicyManagerFactory::PrimaryAccountPolicyManagerFactory()
    : ProfileKeyedServiceFactory(
          "PrimaryAccountPolicyManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
}

PrimaryAccountPolicyManagerFactory::~PrimaryAccountPolicyManagerFactory() =
    default;

std::unique_ptr<KeyedService>
PrimaryAccountPolicyManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<PrimaryAccountPolicyManager>(profile);
}
