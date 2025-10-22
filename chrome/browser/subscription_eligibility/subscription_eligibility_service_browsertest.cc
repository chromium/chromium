// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subscription_eligibility/subscription_eligibility_service.h"

#include <optional>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_prefs.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace subscription_eligibility {

namespace {

class SubscriptionEligibilityServiceObserver
    : public SubscriptionEligibilityService::Observer {
 public:
  std::optional<int32_t> new_subscription_tier() const {
    return new_subscription_tier_;
  }

 private:
  // SubscriptionEligibilityService::Observer:
  void OnAiSubscriptionTierUpdated(int32_t new_subscription_tier) override {
    new_subscription_tier_ = new_subscription_tier;
  }

  std::optional<int32_t> new_subscription_tier_;
};

class SubscriptionEligibilityServiceTest : public InProcessBrowserTest {
 public:
  void SetAiSubscriptionTierForProfile(int32_t subscription_tier) {
    browser()->profile()->GetPrefs()->SetInteger(prefs::kAiSubscriptionTier,
                                                 subscription_tier);
  }

  SubscriptionEligibilityService* service() {
    return SubscriptionEligibilityServiceFactory::GetForProfile(
        browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(SubscriptionEligibilityServiceTest,
                       GetAiSubscriptionTier) {
  EXPECT_EQ(service()->GetAiSubscriptionTier(), 0);

  SubscriptionEligibilityServiceObserver observer;
  service()->AddObserver(&observer);

  SetAiSubscriptionTierForProfile(1);
  EXPECT_EQ(service()->GetAiSubscriptionTier(), 1);

  ASSERT_TRUE(observer.new_subscription_tier());
  EXPECT_EQ(*observer.new_subscription_tier(), 1);
}

}  // namespace

}  // namespace subscription_eligibility
