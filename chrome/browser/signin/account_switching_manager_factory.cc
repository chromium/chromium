// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_switching_manager_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/account_switching_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"

// static
signin::AccountSwitchingManager* AccountSwitchingManagerFactory::GetForProfile(
    Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  return static_cast<signin::AccountSwitchingManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AccountSwitchingManagerFactory* AccountSwitchingManagerFactory::GetInstance() {
  static base::NoDestructor<AccountSwitchingManagerFactory> instance;
  return instance.get();
}

AccountSwitchingManagerFactory::AccountSwitchingManagerFactory()
    : ProfileKeyedServiceFactory("AccountSwitchingManager",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(AccountPreviewDataServiceFactory::GetInstance());
}

AccountSwitchingManagerFactory::~AccountSwitchingManagerFactory() = default;

std::unique_ptr<KeyedService>
AccountSwitchingManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!signin::AccountSwitchingManager::IsFeatureEnabled()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<signin::AccountSwitchingManager>(profile->GetPrefs());
}

void AccountSwitchingManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  signin::AccountSwitchingManager::RegisterProfilePrefs(registry);
}
