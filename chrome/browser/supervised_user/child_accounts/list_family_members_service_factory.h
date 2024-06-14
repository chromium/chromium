// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Do not move to components/ until profile dependency is removed.
#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_LIST_FAMILY_MEMBERS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_LIST_FAMILY_MEMBERS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/supervised_user/core/browser/list_family_members_service.h"
#include "content/public/browser/browser_context.h"

class ListFamilyMembersServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static supervised_user::ListFamilyMembersService* GetForProfile(
      Profile* profile);

  static ListFamilyMembersServiceFactory* GetInstance();

  ListFamilyMembersServiceFactory(const ListFamilyMembersServiceFactory&) =
      delete;
  ListFamilyMembersServiceFactory& operator=(
      const ListFamilyMembersServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ListFamilyMembersServiceFactory>;

  ListFamilyMembersServiceFactory();
  ~ListFamilyMembersServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_LIST_FAMILY_MEMBERS_SERVICE_FACTORY_H_
