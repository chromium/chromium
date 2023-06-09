// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/list_family_members_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/supervised_user/core/common/features.h"

// static
ChildAccountService* ChildAccountServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ChildAccountService*>(
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
          base::FeatureList::IsEnabled(
              supervised_user::kUpdateSupervisedUserFactoryCreation)
              ? supervised_user::BuildProfileSelectionsForRegularAndGuest()
              : supervised_user::BuildProfileSelectionsLegacy()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(SupervisedUserServiceFactory::GetInstance());
  DependsOn(supervised_user::ListFamilyMembersServiceFactory::GetInstance());
}

ChildAccountServiceFactory::~ChildAccountServiceFactory() = default;

KeyedService* ChildAccountServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new ChildAccountService(
      profile,
      supervised_user::ListFamilyMembersServiceFactory::GetForProfile(profile));
}
