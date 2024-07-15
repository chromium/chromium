// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_investigator_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service_factory.h"
#include "components/signin/core/browser/account_investigator.h"
#include "components/signin/public/identity_manager/identity_manager.h"

// static
AccountInvestigatorFactory* AccountInvestigatorFactory::GetInstance() {
  static base::NoDestructor<AccountInvestigatorFactory> instance;
  return instance.get();
}

// static
AccountInvestigator* AccountInvestigatorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AccountInvestigator*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

AccountInvestigatorFactory::AccountInvestigatorFactory()
    : ProfileKeyedServiceFactory(
          "AccountInvestigator",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

AccountInvestigatorFactory::~AccountInvestigatorFactory() = default;

KeyedService* AccountInvestigatorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile(Profile::FromBrowserContext(context));
  AccountInvestigator* investigator = new AccountInvestigator(
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile));
  investigator->Initialize();
  return investigator;
}

void AccountInvestigatorFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AccountInvestigator::RegisterPrefs(registry);
}

bool AccountInvestigatorFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AccountInvestigatorFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
