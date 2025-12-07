// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace subscription_eligibility {
class SubscriptionEligibilityService;

class SubscriptionEligibilityServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static SubscriptionEligibilityService* GetForProfile(Profile* profile);
  static SubscriptionEligibilityServiceFactory* GetInstance();

  SubscriptionEligibilityServiceFactory(
      const SubscriptionEligibilityServiceFactory&) = delete;
  SubscriptionEligibilityServiceFactory& operator=(
      const SubscriptionEligibilityServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SubscriptionEligibilityServiceFactory>;

  SubscriptionEligibilityServiceFactory();
  ~SubscriptionEligibilityServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace subscription_eligibility

#endif  // CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_SERVICE_FACTORY_H_
