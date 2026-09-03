// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/list_family_members_service_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/supervised_user/core/browser/list_family_members_service.h"
#include "content/public/browser/browser_context.h"

namespace supervised_user {

// static
ListFamilyMembersServiceFactory*
ListFamilyMembersServiceFactory::GetInstance() {
  static base::NoDestructor<ListFamilyMembersServiceFactory> instance;
  return instance.get();
}

ListFamilyMembersServiceFactory::ListFamilyMembersServiceFactory()
    : ProfileKeyedServiceFactory("ListFamilyMembersService",
                                 BuildProfileSelectionsForRegularAndGuest()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

ListFamilyMembersServiceFactory::~ListFamilyMembersServiceFactory() = default;

std::unique_ptr<KeyedService>
ListFamilyMembersServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    // Match lifecycle of the identity manager. No identity means no family.
    return nullptr;
  }
  return std::make_unique<ListFamilyMembersService>(
      CHECK_DEREF(identity_manager), profile->GetURLLoaderFactory(),
      CHECK_DEREF(profile->GetPrefs()));
}

bool ListFamilyMembersServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace supervised_user
