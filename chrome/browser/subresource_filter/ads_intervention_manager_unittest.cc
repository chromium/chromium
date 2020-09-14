// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/ads_intervention_manager.h"

#include <memory>

#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using subresource_filter::mojom::AdsViolation;

class AdsInterventionManagerTest : public testing::Test {
 public:
  AdsInterventionManagerTest() = default;
  AdsInterventionManagerTest(const AdsInterventionManagerTest&) = delete;
  AdsInterventionManagerTest& operator=(const AdsInterventionManagerTest&) =
      delete;

  void SetUp() override {
    ads_intervention_manager_ =
        SubresourceFilterProfileContextFactory::GetForProfile(&testing_profile_)
            ->ads_intervention_manager();

    test_clock_ = std::make_unique<base::SimpleTestClock>();
    ads_intervention_manager_->set_clock_for_testing(test_clock_.get());
  }

  base::SimpleTestClock* test_clock() { return test_clock_.get(); }

 protected:
  // Owned by the testing_profile_.
  AdsInterventionManager* ads_intervention_manager_ = nullptr;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;

  std::unique_ptr<base::SimpleTestClock> test_clock_;
};

TEST_F(AdsInterventionManagerTest,
       NoIntervention_NoActiveInterventionReturned) {
  GURL url("https://example.test/");

  base::Optional<AdsInterventionManager::LastAdsIntervention> ads_intervention =
      ads_intervention_manager_->GetLastAdsIntervention(url);
  EXPECT_FALSE(ads_intervention.has_value());
}

TEST_F(AdsInterventionManagerTest, SingleIntervention_TimeSinceMatchesClock) {
  GURL url("https://example.test/");

  ads_intervention_manager_->TriggerAdsInterventionForUrlOnSubsequentLoads(
      url, AdsViolation::kMobileAdDensityByHeightAbove30);
  test_clock()->Advance(base::TimeDelta::FromHours(1));

  base::Optional<AdsInterventionManager::LastAdsIntervention> ads_intervention =
      ads_intervention_manager_->GetLastAdsIntervention(url);
  EXPECT_TRUE(ads_intervention.has_value());
  EXPECT_EQ(ads_intervention->ads_violation,
            AdsViolation::kMobileAdDensityByHeightAbove30);
  EXPECT_EQ(ads_intervention->duration_since, base::TimeDelta::FromHours(1));

  // Advance the clock by two hours, duration since should now be 3 hours.
  test_clock()->Advance(base::TimeDelta::FromHours(2));
  ads_intervention = ads_intervention_manager_->GetLastAdsIntervention(url);
  EXPECT_TRUE(ads_intervention.has_value());
  EXPECT_EQ(ads_intervention->ads_violation,
            AdsViolation::kMobileAdDensityByHeightAbove30);
  EXPECT_EQ(ads_intervention->duration_since, base::TimeDelta::FromHours(3));
}
