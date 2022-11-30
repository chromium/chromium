// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/primary_account_policy_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
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
    : ProfileKeyedServiceFactory("PrimaryAccountPolicyManager") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

PrimaryAccountPolicyManagerFactory::~PrimaryAccountPolicyManagerFactory() =
    default;

KeyedService* PrimaryAccountPolicyManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new PrimaryAccountPolicyManager(profile);
}
