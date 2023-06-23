// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/supervised_user/core/browser/child_account_service.h"

class Profile;

class ChildAccountServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static supervised_user::ChildAccountService* GetForProfile(Profile* profile);

  static ChildAccountServiceFactory* GetInstance();

  ChildAccountServiceFactory(const ChildAccountServiceFactory&) = delete;
  ChildAccountServiceFactory& operator=(const ChildAccountServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<ChildAccountServiceFactory>;

  ChildAccountServiceFactory();
  ~ChildAccountServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_FACTORY_H_
