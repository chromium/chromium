// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/list_family_members_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/common/features.h"

// static
supervised_user::ChildAccountService* ChildAccountServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<supervised_user::ChildAccountService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChildAccountServiceFactory* ChildAccountServiceFactory::GetInstance() {
  static base::NoDestructor<ChildAccountServiceFactory> instance;
  return instance.get();
}

ChildAccountServiceFactory::ChildAccountServiceFactory()
    : ProfileKeyedServiceFactory(
          "ChildAccountService",
          supervised_user::BuildProfileSelectionsForRegularAndGuest()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(ListFamilyMembersServiceFactory::GetInstance());
}

ChildAccountServiceFactory::~ChildAccountServiceFactory() = default;

KeyedService* ChildAccountServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  CHECK(profile->GetPrefs());
  CHECK(ListFamilyMembersServiceFactory::GetForProfile(profile));

  return new supervised_user::ChildAccountService(
      *profile->GetPrefs(),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory(),
      base::BindOnce(&supervised_user::AssertChildStatusOfTheUser, profile),
      *ListFamilyMembersServiceFactory::GetForProfile(profile));
}
