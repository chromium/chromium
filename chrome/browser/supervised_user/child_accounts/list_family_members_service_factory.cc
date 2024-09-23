// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/list_family_members_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/supervised_user/core/browser/list_family_members_service.h"
#include "content/public/browser/browser_context.h"

// static
supervised_user::ListFamilyMembersService*
ListFamilyMembersServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<supervised_user::ListFamilyMembersService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ListFamilyMembersServiceFactory*
ListFamilyMembersServiceFactory::GetInstance() {
  static base::NoDestructor<ListFamilyMembersServiceFactory> instance;
  return instance.get();
}

ListFamilyMembersServiceFactory::ListFamilyMembersServiceFactory()
    : ProfileKeyedServiceFactory(
          "ListFamilyMembersService",
          supervised_user::BuildProfileSelectionsForRegularAndGuest()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

ListFamilyMembersServiceFactory::~ListFamilyMembersServiceFactory() = default;

KeyedService* ListFamilyMembersServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new supervised_user::ListFamilyMembersService(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory(), *profile->GetPrefs());
}
