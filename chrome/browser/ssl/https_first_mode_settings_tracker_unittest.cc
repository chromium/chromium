// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_test_utils.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/site_engagement/core/pref_names.h"
#include "components/user_prefs/user_prefs.h"
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
        CreateProfileForIdentityTestEnvironment(builder);
  }

  void TearDown() override {
    HttpsFirstModeServiceFactory::SetClockForTesting(nullptr);
  }

  void SetSiteEngagementScoreForTypicallySecureUserHeuristic() {
    // Typically Secure User heuristic requires a minimum total site engagement
    // score.
    site_engagement::SiteEngagementService* engagement_service =
        site_engagement::SiteEngagementService::Get(profile());
    ASSERT_TRUE(engagement_service);
    engagement_service->ResetBaseScoreForURL(GURL("https://google.com"), 90);
  }

  TestingProfile* profile() { return profile_.get(); }

  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<TestingProfile> profile_;
};

void MaybeEnableHttpsFirstModeForEngagedSitesAndWait(
    HttpsFirstModeService* hfm_service) {
  base::RunLoop run_loop;
  hfm_service->MaybeEnableHttpsFirstModeForEngagedSites(run_loop.QuitClosure());
  run_loop.Run();
}

// Check that changing the HFM pref clears Site Engagement heuristic's HTTPS
// enforcelist and effectively disables the heuristic.
TEST_F(HttpsFirstModeSettingsTrackerTest,
       SiteEngagementHeuristic_ShouldNotEnableIfPrefIsSet) {
  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

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

  // Site Engagement heuristic should enforce HTTPS on hosts with high
  // engagement score.
  GURL https_url("https://example.com");
  engagement_service->ResetBaseScoreForURL(https_url, 90);
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_TRUE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
  EXPECT_TRUE(state->IsHttpsEnforcedForUrl(
      GURL("https://example.com"), profile()->GetDefaultStoragePartition()));

  // Disable HFM. This should clear the enforcelist.
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      GURL("https://example.com"), profile()->GetDefaultStoragePartition()));

  // Check again. Should remain unenforced.
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      GURL("https://example.com"), profile()->GetDefaultStoragePartition()));
}

// Check that high site engagement scores of HTTPS URLs with non-default ports
// do not auto-enable HTTPS-First Mode.
TEST_F(
    HttpsFirstModeSettingsTrackerTest,
    SiteEngagementHeuristic_ShouldIgnoreEngagementScoreOfUrlWithNonDefaultPort) {
  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  base::HistogramTester histograms;
  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(profile());
  ASSERT_TRUE(engagement_service);

  StatefulSSLHostStateDelegate* state =
      StatefulSSLHostStateDelegateFactory::GetForProfile(profile());
  ASSERT_TRUE(state);

  // HFM should initially be disabled on this site by default.
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 0);
  histograms.ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 0);
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 0);
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 0);

  // Increase the score. Since the URL has a non-default port, HFM should remain
  // disabled.
  engagement_service->ResetBaseScoreForURL(GURL("https://example.com:8443"),
                                           90);
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 0);
  histograms.ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 0);
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 0);
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 0);
}

TEST_F(HttpsFirstModeSettingsTrackerTest,
       SiteEngagementHeuristic_ShouldEnforceHttps) {
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
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 0);
  histograms.ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 0);
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 0);
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 0);

  // Step 2: Increase the score, should now enable HFM.
  engagement_service->ResetBaseScoreForURL(https_url, 90);
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_TRUE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
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
  EXPECT_FALSE(
      state->IsHttpsEnforcedForUrl(GURL("http://test.example.com"),
                                   profile()->GetDefaultStoragePartition()));

  // Step 3: Decrease the score, but only slightly. This shouldn't result in HFM
  // being disabled.
  engagement_service->ResetBaseScoreForURL(https_url, 85);
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_TRUE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
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
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
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
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_TRUE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
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
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
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
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
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
}

// If a site was previously been HTTPS-enforced no longer is in the site
// engagement list, it should no longer be HTTPS-enforced anymore.
TEST_F(HttpsFirstModeSettingsTrackerTest,
       SiteEngagementHeuristic_NoEngagementScoreShouldUnenforceHttps) {
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

  GURL https_url("https://example.com");
  GURL http_url("http://example.com");

  // Increase the HTTPS engagement score to enable HFM on this host.
  engagement_service->ResetBaseScoreForURL(https_url, 90);
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_TRUE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));

  // Clear up the engagement scores. This should remove the enforcement.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  settings_map->SetWebsiteSettingDefaultScope(
      https_url, GURL(), ContentSettingsType::SITE_ENGAGEMENT, base::Value());
  settings_map->SetWebsiteSettingDefaultScope(
      http_url, GURL(), ContentSettingsType::SITE_ENGAGEMENT, base::Value());
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);

  // Sites that fall outside the site engagement list should no longer have
  // HTTPS enforced.
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      GURL("http://example.com"), profile()->GetDefaultStoragePartition()));
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      GURL("https://example.com"), profile()->GetDefaultStoragePartition()));

  service->Shutdown();
}

// Tests the repair mitigation for crbug.com/1475747. Namely, if
// kHttpsFirstModeV2ForTypicallySecureUsers is disabled and the pref is enabled,
// the code should disable HFM to undo the damage done by the bug.
TEST_F(HttpsFirstModeSettingsTrackerTest, UndoTypicallySecureUser) {
  base::test::ScopedFeatureList feature_list;

  feature_list.InitAndDisableFeature(
      features::kHttpsFirstModeV2ForTypicallySecureUsers);

  // Pretend that the feature had been erroneously enabled previously.
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeAutoEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, true);

  HttpsFirstModeService::FixTypicallySecureUserPrefs(profile());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));
}

// Tests the Typically Secure User heuristic to ensure that it respects the
// finch flag. See TypicallySecureUserPref for more details.
// Regression test for crbug.com/1475747.
TEST_F(HttpsFirstModeSettingsTrackerTest,
       TypicallySecureUser_DisabledByDefault) {
  base::test::ScopedFeatureList feature_list;

  feature_list.InitAndDisableFeature(
      features::kHttpsFirstModeV2ForTypicallySecureUsers);

  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  SetSiteEngagementScoreForTypicallySecureUserHeuristic();

  base::SimpleTestClock clock;
  base::Time now = base::Time::NowFromSystemTime();
  clock.SetNow(now);
  service->SetClockForTesting(&clock);
  // Move the clock so that the profile is old enough.
  clock.SetNow(now + base::Days(10));

  // This situation would normally qualify for enabling HFM, but it should stay
  // disabled since the feature is disabled.
  service->RecordHttpsUpgradeFallbackEvent();
  EXPECT_FALSE(service->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));
}

// Creates the HFM service and waits for it to write the initial values into
// the typically secure fallbacks pref.
HttpsFirstModeService*
CreateHttpsFirstModeServiceAndWaitForTypicallySecureUserPrefInitialized(
    Profile* profile,
    base::SimpleTestClock* clock) {
  HttpsFirstModeServiceFactory::SetClockForTesting(clock);
  base::Time now = clock->Now();
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(profile);
  base::Value::Dict expected_pref =
      base::Value::Dict()
          .Set("heuristic_start_timestamp", base::TimeToValue(now))
          .Set("fallback_events", base::Value::List());
  WaitForPrefValue(profile->GetPrefs(), prefs::kHttpsUpgradeFallbacks,
                   base::Value(std::move(expected_pref)));
  return hfm_service;
}

// A new profile shouldn't write any prefs related to Typically Secure User
// heuristic. TypicallySecureUserTest's SetUp() sets an old profile creation
// time and then create HttpsFirstModeService, so this can't be a
// TypicallySecureUserTest.
TEST_F(HttpsFirstModeSettingsTrackerTest, TypicallySecureUser_NewProfile) {
  base::SimpleTestClock clock;
  base::Time now = base::Time::NowFromSystemTime();
  clock.SetNow(now);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kHttpsFirstModeV2ForTypicallySecureUsers);
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  profile()->SetCreationTimeForTesting(now);
  // A new profile will not initialize the heuristic on startup, so no need to
  // wait for kHttpsUpgradeFallbacks pref.

  SetSiteEngagementScoreForTypicallySecureUserHeuristic();
  hfm_service->SetClockForTesting(&clock);

  // kHttpsUpgradeFallbacks is normally written on startup, but not for new
  // profiles. Other prefs shouldn't change.
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsUpgradeFallbacks));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  // A fallback event should be a no-op for a new profile.
  hfm_service->RecordHttpsUpgradeFallbackEvent();
  hfm_service->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstMode();

  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsUpgradeFallbacks));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));
}

// Typically Secure User fallback entries pref written by an older version of
// Chrome should be handled properly by new versions.
TEST_F(HttpsFirstModeSettingsTrackerTest, TypicallySecureUser_OldVersion) {
  // Write an empty pref without the "start_heuristic_timestamp" key.
  base::Value::Dict new_base_pref;
  profile()->GetPrefs()->SetDict(prefs::kHttpsUpgradeFallbacks,
                                 std::move(new_base_pref));
  base::SimpleTestClock clock;
  base::Time now = base::Time::NowFromSystemTime();
  clock.SetNow(now);
  profile()->SetCreationTimeForTesting(now - base::Days(20));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kHttpsFirstModeV2ForTypicallySecureUsers);
  HttpsFirstModeService* hfm_service =
      CreateHttpsFirstModeServiceAndWaitForTypicallySecureUserPrefInitialized(
          profile(), &clock);
  EXPECT_TRUE(hfm_service);

  EXPECT_TRUE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsUpgradeFallbacks));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));
  EXPECT_EQ(0u, hfm_service->GetFallbackEntryCountForTesting());

  // A fallback event should be a no-op as the last fallback event is too
  // recent.
  hfm_service->RecordHttpsUpgradeFallbackEvent();
  hfm_service->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstMode();

  EXPECT_TRUE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsUpgradeFallbacks));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));
  EXPECT_EQ(1u, hfm_service->GetFallbackEntryCountForTesting());
}

class TypicallySecureUserTest : public HttpsFirstModeSettingsTrackerTest {
 public:
  TypicallySecureUserTest() {
    base::FieldTrialParams feature_params;
    feature_params["min-recent-navigations"] = "5";
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kHttpsFirstModeV2ForTypicallySecureUsers, feature_params);
  }

 protected:
  void SetUp() override {
    HttpsFirstModeSettingsTrackerTest::SetUp();

    base::Time now = base::Time::NowFromSystemTime();
    profile()->SetCreationTimeForTesting(now - base::Days(20));
    clock_.SetNow(now);

    SetSiteEngagementScoreForTypicallySecureUserHeuristic();

    hfm_service_ =
        CreateHttpsFirstModeServiceAndWaitForTypicallySecureUserPrefInitialized(
            profile(), &clock_);
    ASSERT_FALSE(
        profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));

    clock_.SetNow(now);
  }

  void RecordFallbackEventAndMaybeEnableHttpsFirstMode() {
    hfm_service()->RecordHttpsUpgradeFallbackEvent();
    hfm_service()->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstMode();
  }
  void IncrementRecentNavigationCount(size_t count) {
    for (size_t i = 0; i < count; i++) {
      hfm_service()->IncrementRecentNavigationCount();
    }
  }

  HttpsFirstModeService* hfm_service() { return hfm_service_; }

  base::SimpleTestClock* clock() { return &clock_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<HttpsFirstModeService> hfm_service_;
  base::SimpleTestClock clock_;
};

// An old profile should initialize the prefs related to Typically Secure User
// heuristic.
TEST_F(TypicallySecureUserTest, ProfileOldEnough) {
  SetSiteEngagementScoreForTypicallySecureUserHeuristic();

  EXPECT_TRUE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsUpgradeFallbacks));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));
  EXPECT_EQ(0u, hfm_service()->GetFallbackEntryCountForTesting());

  // A fallback event should be a no-op as the last fallback event is too
  // recent.
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();

  EXPECT_TRUE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsUpgradeFallbacks));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));
  EXPECT_EQ(1u, hfm_service()->GetFallbackEntryCountForTesting());
}

// Checks that Typically Secure Heuristic must observe navigations for at least
// a week before enabling HFM pref.
TEST_F(TypicallySecureUserTest, EnablePrefWhenObservedForLongEnough) {
  base::Time now = clock()->Now();
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  clock()->SetNow(now + base::Days(3));
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();
  EXPECT_EQ(1u, hfm_service()->GetFallbackEntryCountForTesting());

  // We haven't observed the profile for long enough. HFM shouldn't be enabled
  // yet.
  EXPECT_FALSE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  // Prefs shouldn't be modified yet.
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  clock()->SetNow(now + base::Days(8));
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();
  EXPECT_EQ(2u, hfm_service()->GetFallbackEntryCountForTesting());

  // We have observed for long enough, and we don't have too many fallback
  // events (2). However, last fallback event was too recent, don't enable just
  // yet.
  EXPECT_FALSE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  clock()->SetNow(now + base::Days(9) + base::Hours(1));
  hfm_service()->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstMode();
  EXPECT_EQ(2u, hfm_service()->GetFallbackEntryCountForTesting());

  // Last fallback event is now a day old, but we don't have enough recent
  // navigations. Don't enable yet.
  EXPECT_FALSE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  // Do lots of navigations. Should enable HFM now.
  IncrementRecentNavigationCount(100u);
  hfm_service()->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstMode();
  EXPECT_TRUE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));
}

// Checks that Typically Secure Heuristic must observe navigations for at least
// a week before enabling HFM pref.
TEST_F(TypicallySecureUserTest,
       DontEnablePrefWhenObservedForLongEnoughWithManyWarnings) {
  base::Time now = clock()->Now();
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  clock()->SetNow(now + base::Days(3));
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();

  // We haven't observed the profile for long enough. HFM shouldn't be enabled
  // yet.
  EXPECT_FALSE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  // Prefs shouldn't be modified yet.
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  clock()->SetNow(now + base::Days(8));
  // We have observed for long enough, but we have too many fallback events (3).
  // HFM should not be enabled.
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();
  EXPECT_EQ(3u, hfm_service()->GetFallbackEntryCountForTesting());

  EXPECT_FALSE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));
}

// Tests for the Typically Secure User heuristic. This test repeatedly calls
// RecordHttpsUpgradeFallbackEvent which is normally called
// from HTTPS-Upgrade fallbacks in production code. It then checks if the
// HTTPS-First Mode pref is enabled.
TEST_F(TypicallySecureUserTest, HFMEnabled) {
  IncrementRecentNavigationCount(5u);

  base::Time now = clock()->Now();
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #1. There are no previous HTTPS upgrade fallbacks (i.e.
  // would-be warnings), so this would normally auto-enable HFM, but
  // observation hasn't happened for a long time yet.
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();
  EXPECT_EQ(1u, hfm_service()->GetFallbackEntryCountForTesting());
  EXPECT_FALSE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  // Prefs shouldn't be modified yet.
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  clock()->SetNow(now + base::Days(3));

  // Fallback #2. The fallback entry list is now [#1, #2]. HFM is still not
  // auto-enabled because we haven't observed navigations for long enough yet.
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();
  EXPECT_EQ(2u, hfm_service()->GetFallbackEntryCountForTesting());
  EXPECT_FALSE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  // Prefs shouldn't be modified yet.
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Move forward, this drops fallback #1 as it's now 9 days old.
  clock()->SetNow(now + base::Days(9));
  hfm_service()->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstMode();
  EXPECT_EQ(1u, hfm_service()->GetFallbackEntryCountForTesting());
  // Fallback list is now [#2] and we observed for long enough. HFM will be
  // enabled.
  EXPECT_TRUE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #3. The fallback list is now [#2, #3]. HFM is still auto-enabled.
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();
  EXPECT_EQ(2u, hfm_service()->GetFallbackEntryCountForTesting());
  EXPECT_TRUE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  // Prefs are now set.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #4. The fallback list is now [#2, #3, #4]. This is too many
  // fallbacks, but we don't auto-disable the user pref once it's
  // auto-enabled. (Note that this isn't realistic scenario, as the
  // interstitial is already enabled after fallback #3, and we don't record a
  // fallback when the interstitial is enabled).
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();
  EXPECT_EQ(3u, hfm_service()->GetFallbackEntryCountForTesting());
  EXPECT_TRUE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  // Prefs are still set.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));
}

// Tests the pref update observer callback.
TEST_F(HttpsFirstModeSettingsTrackerTest, PrefUpdated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kHttpsFirstModeIncognito);

  base::HistogramTester histograms;
  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  // Creating the HttpsFirstModeService should emit the "setting at startup"
  // histogram.
  histograms.ExpectUniqueSample(
      "Security.HttpsFirstMode.SettingEnabledAtStartup", false, 1);

  // Enable HTTPS-First Mode pref, check that the setting-change histogram was
  // emitted.
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, true);
  histograms.ExpectTotalCount("Security.HttpsFirstMode.SettingChanged", 1);
  histograms.ExpectBucketCount("Security.HttpsFirstMode.SettingChanged", true,
                               1);

  // Disable the pref and check the histogram.
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);
  histograms.ExpectBucketCount("Security.HttpsFirstMode.SettingChanged", false,
                               1);
}

// Tests the pref update observer callback, with the HttpsFirstModeIncognito
// feature flag enabled (which changes the setting to be a tri-state that
// controls two boolean preferences).
TEST_F(HttpsFirstModeSettingsTrackerTest, PrefUpdatedIncognitoEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHttpsFirstModeIncognito);
  // Pref is registered as true by default.
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsFirstModeIncognito, true);

  base::HistogramTester histograms;
  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  // Creating the HttpsFirstModeService should emit the "setting at startup"
  // histogram.
  histograms.ExpectUniqueSample(
      "Security.HttpsFirstMode.SettingEnabledAtStartup2",
      HttpsFirstModeSetting::kEnabledIncognito, 1);

  // Set prefs as though the user had toggled on "Warn for all insecure
  // navigations". Enable HTTPS-First Mode pref, check that the
  // setting-change histogram was emitted.
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsFirstModeIncognito, true);
  histograms.ExpectTotalCount("Security.HttpsFirstMode.SettingChanged2", 1);
  histograms.ExpectBucketCount("Security.HttpsFirstMode.SettingChanged2",
                               HttpsFirstModeSetting::kEnabledFull, 1);

  // Set prefs as though the user had changed the toggle to "Warn in Incognito".
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsFirstModeIncognito, true);
  histograms.ExpectTotalCount("Security.HttpsFirstMode.SettingChanged2", 2);
  histograms.ExpectBucketCount("Security.HttpsFirstMode.SettingChanged2",
                               HttpsFirstModeSetting::kEnabledIncognito, 1);

  // Disable prefs as though the user had disabled HTTP warnings entirely.
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsFirstModeIncognito, false);
  histograms.ExpectTotalCount("Security.HttpsFirstMode.SettingChanged2", 3);
  histograms.ExpectBucketCount("Security.HttpsFirstMode.SettingChanged2",
                               HttpsFirstModeSetting::kDisabled, 1);
}
