// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"

// static
AccountConsistencyModeManagerFactory*
AccountConsistencyModeManagerFactory::GetInstance() {
  static base::NoDestructor<AccountConsistencyModeManagerFactory> instance;
  return instance.get();
}

// static
AccountConsistencyModeManager*
AccountConsistencyModeManagerFactory::GetForProfile(Profile* profile) {
  DCHECK(profile);
  return static_cast<AccountConsistencyModeManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

AccountConsistencyModeManagerFactory::AccountConsistencyModeManagerFactory()
    : ProfileKeyedServiceFactory(
          "AccountConsistencyModeManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

AccountConsistencyModeManagerFactory::~AccountConsistencyModeManagerFactory() =
    default;

std::unique_ptr<KeyedService>
AccountConsistencyModeManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());
  Profile* profile = Profile::FromBrowserContext(context);

  return std::make_unique<AccountConsistencyModeManager>(profile);
}

void AccountConsistencyModeManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AccountConsistencyModeManager::RegisterProfilePrefs(registry);
}

bool AccountConsistencyModeManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
