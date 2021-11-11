// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ChildAccountService;
class Profile;

class ChildAccountServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ChildAccountService* GetForProfile(Profile* profile);

  static ChildAccountServiceFactory* GetInstance();

  ChildAccountServiceFactory(const ChildAccountServiceFactory&) = delete;
  ChildAccountServiceFactory& operator=(const ChildAccountServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<ChildAccountServiceFactory>;

  ChildAccountServiceFactory();
  ~ChildAccountServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_FACTORY_H_
