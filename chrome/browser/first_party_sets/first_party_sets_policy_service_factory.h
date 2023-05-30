// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace first_party_sets {

class FirstPartySetsPolicyService;
// Singleton that owns FirstPartySetsPolicyService objects and associates them
// with corresponding BrowserContexts.
//
// Listens for the BrowserContext's destruction notification and cleans up the
// associated FirstPartySetsPolicyService.
class FirstPartySetsPolicyServiceFactory : public ProfileKeyedServiceFactory {
 public:
  FirstPartySetsPolicyServiceFactory(
      const FirstPartySetsPolicyServiceFactory&) = delete;
  FirstPartySetsPolicyServiceFactory& operator=(
      const FirstPartySetsPolicyServiceFactory&) = delete;

  static FirstPartySetsPolicyService* GetForBrowserContext(
      content::BrowserContext* context);

  static FirstPartySetsPolicyServiceFactory* GetInstance();

  // Stores `test_factory` to inject test logic into BuildServiceInstanceFor.
  void SetTestingFactoryForTesting(TestingFactory test_factory);

 private:
  friend base::NoDestructor<FirstPartySetsPolicyServiceFactory>;

  FirstPartySetsPolicyServiceFactory();
  ~FirstPartySetsPolicyServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_FACTORY_H_
