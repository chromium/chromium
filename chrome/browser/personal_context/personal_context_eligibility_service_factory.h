// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace personal_context {
class PersonalContextEligibilityService;
}

class Profile;

class PersonalContextEligibilityServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static personal_context::PersonalContextEligibilityService* GetForProfile(
      Profile* profile);
  static PersonalContextEligibilityServiceFactory* GetInstance();

  PersonalContextEligibilityServiceFactory(
      const PersonalContextEligibilityServiceFactory&) = delete;
  PersonalContextEligibilityServiceFactory& operator=(
      const PersonalContextEligibilityServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PersonalContextEligibilityServiceFactory>;

  PersonalContextEligibilityServiceFactory();
  ~PersonalContextEligibilityServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_FACTORY_H_
