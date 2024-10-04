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
#include "chrome/browser/ssl/https_upgrades_util.h"
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
//
// Most of these tests don't need to enable the feature flags for HFM heuristics
// because they call the feature-flag gated functions directly (
// MaybeEnableHttpsFirstModeForEngagedSitesAndWait() and
// CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstMode()).
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
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList feature_list_;
};

class HttpsFirstModeSettingsTrackerSiteEngagementHeuristicTest
    : public HttpsFirstModeSettingsTrackerTest {
 public:
  HttpsFirstModeSettingsTrackerSiteEngagementHeuristicTest() {
    feature_list()->InitWithFeatures(
        /*enabled_features=*/{features::kHttpsFirstModeV2ForEngagedSites,
                              features::kHttpsFirstBalancedMode},
        /*disabled_features=*/{});
  }
};

void MaybeEnableHttpsFirstModeForEngagedSitesAndWait(
    HttpsFirstModeService* hfm_service) {
  base::RunLoop run_loop;
  hfm_service->MaybeEnableHttpsFirstModeForEngagedSites(run_loop.QuitClosure());
  run_loop.Run();
}

// Check that changing the HFM pref clears Site Engagement heuristic's HTTPS
// enforcelist and effectively disables the heuristic.
TEST_F(HttpsFirstModeSettingsTrackerSiteEngagementHeuristicTest,
       ShouldNotEnableHeuristicIfPrefIsSet) {
  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

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
  // engagement score if Balanced Mode is available.
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
// do not auto-enable HTTPS-First Balanced Mode.
TEST_F(HttpsFirstModeSettingsTrackerSiteEngagementHeuristicTest,
       ShouldIgnoreEngagementScoreOfUrlWithNonDefaultPort) {
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

TEST_F(HttpsFirstModeSettingsTrackerSiteEngagementHeuristicTest,
       BalancedModeExceptions) {
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

  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  state->SetClockForTesting(std::move(clock));
  clock_ptr->SetNow(base::Time::NowFromSystemTime());

  // HTTPS-First Balanced Mode should ignore non-unique hostnames.
  GURL https_url("https://site.test");
  GURL http_url("http://site.test");

  // HFM should initially be disabled on this site by default.
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      http_url, profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 0);
  histograms.ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 0);
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 0);
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 0);

  // Increase the score. This should still not enable HFM on site.test.
  engagement_service->ResetBaseScoreForURL(https_url, 90);
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);

  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      http_url, profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 0);
  histograms.ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 0);
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 0);
  histograms.ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 0);
}

TEST_F(HttpsFirstModeSettingsTrackerSiteEngagementHeuristicTest,
       ShouldEnforceHttps) {
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

// If a site that was previously HTTPS-enforced is no longer in the site
// engagement list, it should no longer be HTTPS-enforced anymore.
TEST_F(HttpsFirstModeSettingsTrackerSiteEngagementHeuristicTest,
       NoEngagementScoreShouldUnenforceHttps) {
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

  GURL https_url("https://example.com");
  GURL http_url("http://example.com");

  // Increase the HTTPS engagement score. This will enable HFM Balanced Mode on
  // this host if available.
  engagement_service->ResetBaseScoreForURL(https_url, 90);
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_TRUE(state->IsHttpsEnforcedForUrl(
      http_url, profile()->GetDefaultStoragePartition()));

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
      http_url, profile()->GetDefaultStoragePartition()));
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      https_url, profile()->GetDefaultStoragePartition()));
}

// If a non-unique hostname was HTTPS-enforced by a previous version of Chrome,
// it should be removed from the enforcement list when possible.
TEST_F(HttpsFirstModeSettingsTrackerSiteEngagementHeuristicTest,
       PreviouslyEnforcedNonUniqueHostnameShouldBeRemoved) {
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

  GURL http_url("http://site.test");
  state->SetHttpsEnforcementForHost(http_url.host(), /*enforce=*/true,
                                    profile()->GetDefaultStoragePartition());

  // The heuristic should clean up the enforcement list and remove non-unique
  // hostnames.
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(service);
  EXPECT_FALSE(state->IsHttpsEnforcedForUrl(
      http_url, profile()->GetDefaultStoragePartition()));
}

// Tests the Typically Secure User heuristic to ensure that it respects the
// finch flag. See TypicallySecureUserPref for more details.
// Regression test for crbug.com/1475747.
TEST_F(HttpsFirstModeSettingsTrackerSiteEngagementHeuristicTest,
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
  hfm_service->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode();

  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsUpgradeFallbacks));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));
}

// Typically Secure User fallback entries pref written by an older version of
// Chrome should be handled properly by new versions.
// This can't be a TypicallySecureUserTest because we need to write the prefs
// before creating the service.
TEST_F(HttpsFirstModeSettingsTrackerTest, TypicallySecureUser_OldVersion) {
  // Write an empty pref without the "start_heuristic_timestamp" key.
  base::Value::Dict new_base_pref;
  profile()->GetPrefs()->SetDict(prefs::kHttpsUpgradeFallbacks,
                                 std::move(new_base_pref));
  base::SimpleTestClock clock;
  base::Time now = base::Time::NowFromSystemTime();
  clock.SetNow(now);
  profile()->SetCreationTimeForTesting(now - base::Days(20));

  // Enable the feature flags explicitly for this test.
  feature_list()->InitWithFeatures(
      /*enabled_features=*/{features::kHttpsFirstModeV2ForTypicallySecureUsers,
                            features::kHttpsFirstBalancedMode},
      /*disabled_features=*/{});

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
  hfm_service->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode();

  EXPECT_TRUE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsUpgradeFallbacks));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));
  EXPECT_EQ(1u, hfm_service->GetFallbackEntryCountForTesting());
}

class HttpsFirstModeSettingsTrackerTypicallySecureUserTest
    : public HttpsFirstModeSettingsTrackerTest {
 public:
  HttpsFirstModeSettingsTrackerTypicallySecureUserTest() {
    base::FieldTrialParams feature_params;
    feature_params["min-recent-navigations"] = "5";
    feature_list()->InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::
                                   kHttpsFirstModeV2ForTypicallySecureUsers,
                               feature_params},
                              {features::kHttpsFirstBalancedMode, {}}},
        /*disabled_features=*/{});
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
    hfm_service()
        ->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode();
  }
  void IncrementRecentNavigationCount(size_t count) {
    for (size_t i = 0; i < count; i++) {
      hfm_service()->IncrementRecentNavigationCount();
    }
  }

  HttpsFirstModeService* hfm_service() { return hfm_service_; }

  base::SimpleTestClock* clock() { return &clock_; }

 private:
  raw_ptr<HttpsFirstModeService> hfm_service_;
  base::SimpleTestClock clock_;
};

// An old profile should initialize the prefs related to Typically Secure User
// heuristic.
TEST_F(HttpsFirstModeSettingsTrackerTypicallySecureUserTest, ProfileOldEnough) {
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
TEST_F(HttpsFirstModeSettingsTrackerTypicallySecureUserTest,
       EnablePrefWhenObservedForLongEnough) {
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
  hfm_service()
      ->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode();
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
  hfm_service()
      ->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode();
  EXPECT_TRUE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsFirstBalancedMode));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));
  // Shouldn't modify strict mode pref:
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
}

// Checks that Typically Secure Heuristic must observe navigations for at least
// a week before enabling HFM pref.
TEST_F(HttpsFirstModeSettingsTrackerTypicallySecureUserTest,
       DontEnablePrefWhenObservedForLongEnoughWithManyWarnings) {
  base::Time now = clock()->Now();
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsFirstBalancedMode));
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
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsFirstBalancedMode));
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
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsFirstBalancedMode));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));
}

// Tests the usual flow of the Typically Secure User heuristic. Repeatedly calls
// RecordHttpsUpgradeFallbackEvent which is normally called
// from HTTPS-Upgrade fallbacks in production code. It then checks if the
// HTTPS-First Balanced Mode pref is enabled as expected.
TEST_F(HttpsFirstModeSettingsTrackerTypicallySecureUserTest,
       BalancedModeEnabled) {
  IncrementRecentNavigationCount(5u);

  base::Time now = clock()->Now();
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsFirstBalancedMode));
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
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsFirstBalancedMode));
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
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsFirstBalancedMode));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Move forward, this drops fallback #1 as it's now 9 days old.
  clock()->SetNow(now + base::Days(9));
  hfm_service()
      ->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode();
  EXPECT_EQ(1u, hfm_service()->GetFallbackEntryCountForTesting());
  // Fallback list is now [#2] and we observed for long enough. HFM will be
  // enabled.
  EXPECT_TRUE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsFirstBalancedMode));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));

  // Fallback #3. The fallback list is now [#2, #3]. Balanced Mode is still
  // auto-enabled.
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();
  EXPECT_EQ(2u, hfm_service()->GetFallbackEntryCountForTesting());
  EXPECT_TRUE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  // Prefs are now set.
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsFirstBalancedMode));
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
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsFirstBalancedMode));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeAutoEnabled));
}

// Checks that manually changing the HFM pref in the UI clears the HTTP
// allowlist. A variant of this test
// (TypicallySecureUserTest.PrefUpdatedByHeuristic_ShouldNotClearAllowlist)
// checks that a heuristic auto-enabling HFM does NOT clear the allowlist.
TEST_F(HttpsFirstModeSettingsTrackerTest, PrefUpdated_ShouldClearAllowlist) {
  feature_list()->InitAndDisableFeature(features::kHttpsFirstBalancedMode);

  // Instantiate the service so that it can track pref changes.
  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));

  // Allowlist a host for for http.
  StatefulSSLHostStateDelegate* state =
      StatefulSSLHostStateDelegateFactory::GetForProfile(profile());
  ASSERT_TRUE(state);
  content::StoragePartition* storage_partition =
      profile()->GetDefaultStoragePartition();
  state->AllowHttpForHost("http-allowed.com", storage_partition);
  EXPECT_TRUE(
      state->IsHttpAllowedForHost("http-allowed.com", storage_partition));

  // Change the UI setting. This should clear the http allowlist.
  profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled, true);
  EXPECT_FALSE(
      state->IsHttpAllowedForHost("http-allowed.com", storage_partition));
}

TEST_F(HttpsFirstModeSettingsTrackerTypicallySecureUserTest,
       PrefUpdatedByHeuristic_ShouldNotClearAllowlist) {
  StatefulSSLHostStateDelegate* state =
      StatefulSSLHostStateDelegateFactory::GetForProfile(profile());
  ASSERT_TRUE(state);
  content::StoragePartition* storage_partition =
      profile()->GetDefaultStoragePartition();
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  state->AllowHttpForHost("http-allowed.com", storage_partition);
  EXPECT_TRUE(
      state->IsHttpAllowedForHost("http-allowed.com", storage_partition));

  // From here on, do a bunch of navigations and advance the clock so that
  // Typically Secure heuristic eventually auto-enables HFM.

  base::Time now = clock()->Now();
  clock()->SetNow(now + base::Days(3));
  // Record a fallback event to start the Typically Secure observation.
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();
  EXPECT_EQ(1u, hfm_service()->GetFallbackEntryCountForTesting());

  // Move forward and record another event.
  clock()->SetNow(now + base::Days(8));
  RecordFallbackEventAndMaybeEnableHttpsFirstMode();
  EXPECT_EQ(2u, hfm_service()->GetFallbackEntryCountForTesting());

  // We have observed for long enough, and we don't have too many fallback
  // events (2). However, last fallback event was too recent. Move forward
  // again.
  clock()->SetNow(now + base::Days(9) + base::Hours(1));
  hfm_service()
      ->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode();
  EXPECT_EQ(2u, hfm_service()->GetFallbackEntryCountForTesting());

  // Last fallback event is now a day old, but we don't have enough recent
  // navigations. Don't enable yet.
  EXPECT_FALSE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsFirstBalancedMode));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  // Finally, do lots of navigations. Should auto-enable HFM now.
  IncrementRecentNavigationCount(100u);
  hfm_service()
      ->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode();
  EXPECT_TRUE(
      hfm_service()->IsInterstitialEnabledByTypicallySecureUserHeuristic());
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsFirstBalancedMode));
  EXPECT_TRUE(
      profile()->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));

  // Typically Secure heuristic auto-enabling HFM should not clear the
  // allowlist.
  EXPECT_TRUE(
      state->IsHttpAllowedForHost("http-allowed.com", storage_partition));
}

// Tests that the correct setting at startup is logged, when the Balanced Mode
// feature flag is enabled but not on by default.
TEST_F(HttpsFirstModeSettingsTrackerTest, StartupBalancedModeAvailable) {
  feature_list()->InitAndEnableFeature(features::kHttpsFirstBalancedMode);

  base::HistogramTester histograms;
  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  // Creating the HttpsFirstModeService should emit the "setting at startup"
  // histogram.
  histograms.ExpectUniqueSample(
      "Security.HttpsFirstMode.SettingEnabledAtStartup2",
      HttpsFirstModeSetting::kDisabled, 1);
}

// Tests that the correct setting at startup is logged, when Balanced Mode
// is auto-enabled.
TEST_F(HttpsFirstModeSettingsTrackerTest, StartupBalancedModeAutoEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kHttpsFirstBalancedMode,
                            features::kHttpsFirstBalancedModeAutoEnable},
      /*disabled_features=*/{});

  base::HistogramTester histograms;
  HttpsFirstModeService* service =
      HttpsFirstModeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  // Creating the HttpsFirstModeService should emit the "setting at startup"
  // histogram.
  histograms.ExpectUniqueSample(
      "Security.HttpsFirstMode.SettingEnabledAtStartup2",
      HttpsFirstModeSetting::kEnabledBalanced, 1);
}
