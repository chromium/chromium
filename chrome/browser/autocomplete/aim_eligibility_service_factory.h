// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class AimEligibilityService;
class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

class AimEligibilityServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AimEligibilityService* GetForProfile(Profile* profile);
  static AimEligibilityServiceFactory* GetInstance();

  // Returns the default factory to pass to `SetTestingFactory()` for testing.
  static TestingFactory GetDefaultFactory();

  AimEligibilityServiceFactory(const AimEligibilityServiceFactory&) = delete;
  AimEligibilityServiceFactory& operator=(const AimEligibilityServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<AimEligibilityServiceFactory>;

  AimEligibilityServiceFactory();
  ~AimEligibilityServiceFactory() override;

  // Overrides from BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_FACTORY_H_
