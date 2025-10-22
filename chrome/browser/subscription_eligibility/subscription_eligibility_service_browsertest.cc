// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subscription_eligibility/subscription_eligibility_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_prefs.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace subscription_eligibility {

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

  SetAiSubscriptionTierForProfile(1);
  EXPECT_EQ(service()->GetAiSubscriptionTier(), 1);
}

}  // namespace subscription_eligibility
