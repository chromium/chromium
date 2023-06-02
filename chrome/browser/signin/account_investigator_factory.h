// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_INVESTIGATOR_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_INVESTIGATOR_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class AccountInvestigator;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

// Factory for BrowserKeyedService AccountInvestigator.
class AccountInvestigatorFactory : public ProfileKeyedServiceFactory {
 public:
  static AccountInvestigator* GetForProfile(Profile* profile);

  static AccountInvestigatorFactory* GetInstance();

  AccountInvestigatorFactory(const AccountInvestigatorFactory&) = delete;
  AccountInvestigatorFactory& operator=(const AccountInvestigatorFactory&) =
      delete;

 private:
  friend base::NoDestructor<AccountInvestigatorFactory>;

  AccountInvestigatorFactory();
  ~AccountInvestigatorFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_INVESTIGATOR_FACTORY_H_
