// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subscription_eligibility/subscription_eligibility_service.h"

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_metrics_provider.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_prefs.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

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
  void SetAiSubscriptionTierForProfile(int32_t subscription_tier,
                                       Profile* profile = nullptr) {
    if (!profile) {
      profile = browser()->profile();
    }
    profile->GetPrefs()->SetInteger(prefs::kAiSubscriptionTier,
                                    subscription_tier);
  }

  SubscriptionEligibilityService* service() {
    return SubscriptionEligibilityServiceFactory::GetForProfile(
        browser()->profile());
  }

  // Explicitly calls ProvideCurrentSessionData() for all metrics providers.
  void ProvideCurrentSessionData() {
    // The purpose of the below call is to avoid a DCHECK failure in an
    // unrelated metrics provider, in
    // |FieldTrialsProvider::ProvideCurrentSessionData()|.
    metrics::SystemProfileProto system_profile_proto;
    g_browser_process->metrics_service()
        ->GetDelegatingProviderForTesting()
        ->ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks::Now(),
                                                         &system_profile_proto);
    metrics::ChromeUserMetricsExtension uma_proto;
    g_browser_process->metrics_service()
        ->GetDelegatingProviderForTesting()
        ->ProvideCurrentSessionData(&uma_proto);
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

// We don't run this test on ChromeOS because we can't create multiple
// profiles.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SubscriptionEligibilityServiceTest, Metrics) {
  {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(service()->GetAiSubscriptionTier(), 0);
    ProvideCurrentSessionData();
    histogram_tester.ExpectUniqueSample(
        "SubscriptionEligibility.AiSubscriptionTierStatus",
        subscription_eligibility::AiSubscriptionTierStatus::
            kNoProfilesSubscribed,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    SetAiSubscriptionTierForProfile(1);
    EXPECT_EQ(service()->GetAiSubscriptionTier(), 1);
    ProvideCurrentSessionData();
    histogram_tester.ExpectUniqueSample(
        "SubscriptionEligibility.AiSubscriptionTierStatus",
        subscription_eligibility::AiSubscriptionTierStatus::
            kAllProfilesAtTierEquals1,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    SetAiSubscriptionTierForProfile(2);
    EXPECT_EQ(service()->GetAiSubscriptionTier(), 2);
    ProvideCurrentSessionData();
    histogram_tester.ExpectUniqueSample(
        "SubscriptionEligibility.AiSubscriptionTierStatus",
        subscription_eligibility::AiSubscriptionTierStatus::
            kAllProfilesAtTierEquals2,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    SetAiSubscriptionTierForProfile(3);
    EXPECT_EQ(service()->GetAiSubscriptionTier(), 3);
    ProvideCurrentSessionData();
    histogram_tester.ExpectUniqueSample(
        "SubscriptionEligibility.AiSubscriptionTierStatus",
        subscription_eligibility::AiSubscriptionTierStatus::
            kAllProfilesAtTierEquals3,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    SetAiSubscriptionTierForProfile(INT_MAX);
    EXPECT_EQ(service()->GetAiSubscriptionTier(), INT_MAX);
    ProvideCurrentSessionData();
    histogram_tester.ExpectUniqueSample(
        "SubscriptionEligibility.AiSubscriptionTierStatus",
        subscription_eligibility::AiSubscriptionTierStatus::
            kAllProfilesSubscribedForUnknownTier,
        1);
  }

  // Add another profile. The default value for subscription tier is 0.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  Profile* second_profile =
      &profiles::testing::CreateProfileSync(profile_manager, new_path);

  {
    base::HistogramTester histogram_tester;
    SetAiSubscriptionTierForProfile(1, browser()->profile());
    SetAiSubscriptionTierForProfile(0, second_profile);
    ProvideCurrentSessionData();
    histogram_tester.ExpectUniqueSample(
        "SubscriptionEligibility.AiSubscriptionTierStatus",
        subscription_eligibility::AiSubscriptionTierStatus::
            kSomeProfilesSubscribed,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    SetAiSubscriptionTierForProfile(1, browser()->profile());
    SetAiSubscriptionTierForProfile(2, second_profile);
    ProvideCurrentSessionData();
    histogram_tester.ExpectUniqueSample(
        "SubscriptionEligibility.AiSubscriptionTierStatus",
        subscription_eligibility::AiSubscriptionTierStatus::
            kAllProfilesSubscribedButDifferentTiers,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    SetAiSubscriptionTierForProfile(1, browser()->profile());
    SetAiSubscriptionTierForProfile(1, second_profile);
    ProvideCurrentSessionData();
    histogram_tester.ExpectUniqueSample(
        "SubscriptionEligibility.AiSubscriptionTierStatus",
        subscription_eligibility::AiSubscriptionTierStatus::
            kAllProfilesAtTierEquals1,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    SetAiSubscriptionTierForProfile(0, browser()->profile());
    // Intentionally throw in a bad value and treat it as not subscribed.
    SetAiSubscriptionTierForProfile(-1, second_profile);
    ProvideCurrentSessionData();
    histogram_tester.ExpectUniqueSample(
        "SubscriptionEligibility.AiSubscriptionTierStatus",
        subscription_eligibility::AiSubscriptionTierStatus::
            kNoProfilesSubscribed,
        1);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace

}  // namespace subscription_eligibility
