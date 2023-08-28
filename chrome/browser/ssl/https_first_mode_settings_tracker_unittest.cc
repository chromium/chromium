// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using security_interstitials::https_only_mode::
    kSiteEngagementHeuristicAccumulatedHostCountHistogram;
using security_interstitials::https_only_mode::
    kSiteEngagementHeuristicEnforcementDurationHistogram;
using security_interstitials::https_only_mode::
    kSiteEngagementHeuristicHostCountHistogram;
using security_interstitials::https_only_mode::
    kSiteEngagementHeuristicStateHistogram;
using security_interstitials::https_only_mode::SiteEngagementHeuristicState;

// Tests for HTTPS First Mode settings, such as enabling HFM through Site
// Engagement scores.
class HttpsFirstModeSettingsTrackerTest : public testing::Test {
 protected:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        StatefulSSLHostStateDelegateFactory::GetInstance(),
        StatefulSSLHostStateDelegateFactory::GetDefaultFactoryForTesting());
    builder.AddTestingFactory(
        safe_browsing::AdvancedProtectionStatusManagerFactory::GetInstance(),
        safe_browsing::AdvancedProtectionStatusManagerFactory::
            GetDefaultFactoryForTesting());
    builder.AddTestingFactory(
        HttpsFirstModeServiceFactory::GetInstance(),
        HttpsFirstModeServiceFactory::GetDefaultFactoryForTesting());
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(
            builder, signin::AccountConsistencyMethod::kMirror);
  }

  TestingProfile* profile() { return profile_.get(); }

  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(HttpsFirstModeSettingsTrackerTest, SiteEngagementHeuristic) {
  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kHttpsFirstModeV2ForTypicallySecureUsers);

  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(profile());
  ASSERT_TRUE(engagement_service);

  StatefulSSLHostStateDelegate* state =
      StatefulSSLHostStateDelegateFactory::GetForProfile(profile());
  ASSERT_TRUE(state);

  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  state->SetClockForTesting(std::move(clock));
  clock_ptr->SetNow(base::Time::NowFromSystemTime());

  GURL https_url("https://example.com");
  GURL http_url("http://example.com");

  // Step 1: HFM should initially be disabled on this site by default.
  service->MaybeEnableHttpsFirstModeForUrl(https_url);
  EXPECT_FALSE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 0);
  histograms.ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 0);
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 0);
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 0);

  // Step 2: Increase the score, should now enable HFM.
  engagement_service->ResetBaseScoreForURL(https_url, 90);
  service->MaybeEnableHttpsFirstModeForUrl(https_url);
  EXPECT_TRUE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  // Check events.
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kEnabled, 1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kDisabled, 0);
  // Check host counts.
  histograms.ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                               /*sample=*/1, /*expected_count=*/1);
  // Check accumulated host counts.
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 1);
  histograms.ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram,
      /*sample=*/1, /*expected_count=*/1);
  // Check enforcement durations. Enforcement duration is only recorded when a
  // host is removed from the list.
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 0);

  // Subdomains shouldn't be affected.
  EXPECT_FALSE(state->IsHttpsEnforcedForHost(
      "test.example.com", profile()->GetDefaultStoragePartition()));

  // Step 3: Decrease the score, but only slightly. This shouldn't result in HFM
  // being disabled.
  engagement_service->ResetBaseScoreForURL(https_url, 85);
  service->MaybeEnableHttpsFirstModeForUrl(https_url);
  EXPECT_TRUE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  // Check events.
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kEnabled, 1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kDisabled, 0);
  // Check host counts.
  histograms.ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                               /*sample=*/1, /*expected_count=*/1);
  // Check accumulated host counts.
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 1);
  histograms.ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram,
      /*sample=*/1, /*expected_count=*/1);
  // Check enforcement durations. Enforcement duration is only recorded when a
  // host is removed from the list.
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 0);

  // Step 4: Decrease the score further. This should result in HFM being
  // disabled. Also move the time forward.
  clock_ptr->Advance(base::Hours(1));
  engagement_service->ResetBaseScoreForURL(https_url, 25);
  service->MaybeEnableHttpsFirstModeForUrl(https_url);
  EXPECT_FALSE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  // Check events.
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 2);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kEnabled, 1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kDisabled, 1);
  // Check host counts.
  histograms.ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 2);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                               /*sample=*/0, /*expected_count=*/1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                               /*sample=*/1, /*expected_count=*/1);
  // Check accumulated host counts.
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 2);
  histograms.ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram,
      /*sample=*/0, /*expected_count=*/0);
  histograms.ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram,
      /*sample=*/1, /*expected_count=*/2);
  // Check enforcement durations.
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 1);
  histograms.ExpectTimeBucketCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, base::Hours(1), 1);

  // Step 5: Increase the score again and re-enable HFM.
  clock_ptr->Advance(base::Hours(2));
  engagement_service->ResetBaseScoreForURL(https_url, 90);
  service->MaybeEnableHttpsFirstModeForUrl(https_url);
  EXPECT_TRUE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  // Check state.
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 3);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kEnabled, 2);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kDisabled, 1);
  // Check host counts.
  histograms.ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 3);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                               /*sample=*/0, /*expected_count=*/1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                               /*sample=*/1, /*expected_count=*/2);
  // Check accumulated host counts.
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 3);
  histograms.ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram,
      /*sample=*/0, /*expected_count=*/0);
  histograms.ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram,
      /*sample=*/1, /*expected_count=*/3);
  // Check enforcement durations.
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 1);
  histograms.ExpectTimeBucketCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, base::Hours(1), 1);

  // Step 6: Also increase the HTTP score. This should disable HFM even though
  // the HTTPS score is still high.
  engagement_service->ResetBaseScoreForURL(http_url, 20);
  service->MaybeEnableHttpsFirstModeForUrl(https_url);
  EXPECT_FALSE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  // Check state.
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 4);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kEnabled, 2);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kDisabled, 2);
  // Check host count.
  histograms.ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 4);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                               /*sample=*/0, /*expected_count=*/2);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                               /*sample=*/1, /*expected_count=*/2);
  // Check accumulated host counts.
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 4);
  histograms.ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram,
      /*sample=*/0, /*expected_count=*/0);
  histograms.ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram,
      /*sample=*/1, /*expected_count=*/4);
  // Check enforcement durations.
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 2);
  histograms.ExpectTimeBucketCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, base::Hours(1), 1);
  histograms.ExpectTimeBucketCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, base::Hours(2), 1);

  // Step 7: Set HTTP score to max and set HTTPS score to zero. This simulates
  // the user spending their time on the HTTP URL and the HTTPS score decaying
  // over time.
  engagement_service->ResetBaseScoreForURL(https_url, 0);
  engagement_service->ResetBaseScoreForURL(http_url, 100);
  service->MaybeEnableHttpsFirstModeForUrl(https_url);
  EXPECT_FALSE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  // Check state.
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 4);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kEnabled, 2);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kDisabled, 2);
  // Check host counts.
  histograms.ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 4);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                               /*sample=*/0, /*expected_count=*/2);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                               /*sample=*/1, /*expected_count=*/2);
  // Check accumulated host counts.
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 4);
  histograms.ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram,
      /*sample=*/0, /*expected_count=*/0);
  histograms.ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram,
      /*sample=*/1, /*expected_count=*/4);
  // Check enforcement durations.
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 2);
  histograms.ExpectTimeBucketCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, base::Hours(1), 1);
  histograms.ExpectTimeBucketCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, base::Hours(2), 1);

  service->Shutdown();
}

// Tests the repair mitigation for crbug.com/1475747. Namely, if
// kHttpsFirstModeV2ForTypicallySecureUsers is disabled and the pref is enabled,
// the code should disable HFM to undo the damage done by the bug.
TEST_F(HttpsFirstModeSettingsTrackerTest, UndoTypicallySecureUser) {
  base::test::ScopedFeatureList feature_list;

  feature_list.InitAndDisableFeature(
      features::kHttpsFirstModeV2ForTypicallySecureUsers);

  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  // Pretend that the feature had been erroneously enabled previously.
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeAutoEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, true);

  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));
}

// Tests the Typically Secure User heuristic to ensure that it respects the
// finch flag. See TypicallySecureUserPref for more details.
// Regression test for crbug.com/1475747.
TEST_F(HttpsFirstModeSettingsTrackerTest,
       TypicallySecureUserDisabledByDefault) {
  base::test::ScopedFeatureList feature_list;

  feature_list.InitAndDisableFeature(
      features::kHttpsFirstModeV2ForTypicallySecureUsers);

  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  // Typically Secure User heuristic requires a minimum total site engagement
  // score.
  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(profile());
  ASSERT_TRUE(engagement_service);
  engagement_service->ResetBaseScoreForURL(GURL("https://google.com"), 90);

  base::SimpleTestClock clock;
  base::Time now = base::Time::NowFromSystemTime();
  clock.SetNow(now);
  service->SetClockForTesting(&clock);
  // Move the clock so that the profile is old enough.
  clock.SetNow(now + base::Days(10));

  // This situation would normally qualify for enabling HFM, but it should stay
  // disabled since the feature is disabled.
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/false));
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));
}

// Tests for the Typically Secure User heuristic. This test repeatedly calls
// MaybeEnableHttpsFirstModeForUser which is normally called from HTTPS-Upgrade
// fallbacks in production code. It then checks if the HTTPS-First Mode pref
// is enabled.
TEST_F(HttpsFirstModeSettingsTrackerTest, TypicallySecureUserPref) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kHttpsFirstModeV2ForTypicallySecureUsers);

  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  // Typically Secure User heuristic requires a minimum total site engagement
  // score.
  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(profile());
  ASSERT_TRUE(engagement_service);
  engagement_service->ResetBaseScoreForURL(GURL("https://google.com"), 90);

  base::SimpleTestClock clock;
  base::Time now = base::Time::NowFromSystemTime();
  clock.SetNow(now);
  service->SetClockForTesting(&clock);

  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #1. There are no previous HTTPS upgrade fallbacks (i.e.
  // would-be warnings), so this would normally auto-enable HFM, but the profile
  // age is not old enough.
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/false));
  // Prefs shouldn't be modified yet.
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  // Move the clock so that the profile is old enough. This also drops entry #1
  // since it's now too old.
  clock.SetNow(now + base::Days(10));
  // Fallback #2. There's now only one would-be warning, HFM is auto-enabled.
  EXPECT_TRUE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_TRUE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/false));
  // Prefs are now set.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #3. There is now two recent would-be warnings. HFM is still
  // auto-enabled.
  EXPECT_TRUE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_TRUE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/false));
  // Prefs are still set.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #4. There is now three recent would-be warnings. HFM is no longer
  // auto-enabled.
  clock.SetNow(now + base::Days(11));
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/false));
  // Prefs are still set. We don't auto-disable the user pref once it's
  // auto-enabled.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #5. No change the next day.
  clock.SetNow(now + base::Days(12));
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/false));
  // Prefs are still set. We don't auto-disable the user pref once it's
  // auto-enabled.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #6 on the 17th day.
  clock.SetNow(now + base::Days(17));
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/false));
  // Prefs are still set. We don't auto-disable the user pref once it's
  // auto-enabled.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #7 on the 18th day. This drops previous entries (2, 3 and 4). We
  // now have entries 5, 6 and 7. This is still too many would-be warnings to
  // auto-enable HFM.
  clock.SetNow(now + base::Days(18));
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/false));
  // Prefs are still set. We don't auto-disable the user pref once it's
  // auto-enabled.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #8 on the 25th day. This drops entries 5 and 6. We have 7 and 8.
  // HFM is now auto-enabled.
  clock.SetNow(now + base::Days(25));
  EXPECT_TRUE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_TRUE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/false));
  // Prefs are still set. We don't auto-disable the user pref once it's
  // auto-enabled.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #9 on the 40th day. This drops all previous entries. Since there
  // is only one entry now, this auto-enables HFM.
  clock.SetNow(now + base::Days(40));
  EXPECT_TRUE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_TRUE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/false));
  // Prefs are still set. We don't auto-disable the user pref once it's
  // auto-enabled.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #10 on the 41st day. HFM is still auto-enabled.
  clock.SetNow(now + base::Days(41));
  EXPECT_TRUE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_TRUE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/false));
  // Prefs are still set. We don't auto-disable the user pref once it's
  // auto-enabled.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #11 on the 42st day. There's now three would-be warnings (9, 10,
  // 11). HFM is not auto-enabled.
  clock.SetNow(now + base::Days(42));
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/true));
  EXPECT_FALSE(
      service->MaybeEnableHttpsFirstModeForUser(/*add_fallback_entry=*/false));
  // Prefs are still set. We don't auto-disable the user pref once it's
  // auto-enabled.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Disable the HFM pref. This should also disable auto-enable pref.
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));
}
