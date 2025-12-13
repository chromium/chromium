// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service.h"

namespace subscription_eligibility {

// static
SubscriptionEligibilityService*
SubscriptionEligibilityServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SubscriptionEligibilityService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SubscriptionEligibilityServiceFactory*
SubscriptionEligibilityServiceFactory::GetInstance() {
  static base::NoDestructor<SubscriptionEligibilityServiceFactory> instance;
  return instance.get();
}

SubscriptionEligibilityServiceFactory::SubscriptionEligibilityServiceFactory()
    : ProfileKeyedServiceFactory(
          "SubscriptionEligibilityService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

SubscriptionEligibilityServiceFactory::
    ~SubscriptionEligibilityServiceFactory() = default;

std::unique_ptr<KeyedService>
SubscriptionEligibilityServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<SubscriptionEligibilityService>(profile->GetPrefs());
}

}  // namespace subscription_eligibility
