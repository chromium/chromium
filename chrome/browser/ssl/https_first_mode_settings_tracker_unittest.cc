// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "base/json/values_util.h"
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
#include "components/prefs/pref_test_utils.h"
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
        CreateProfileForIdentityTestEnvironment(builder);
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
    feature_list_.InitAndEnableFeature(
        features::kHttpsFirstModeV2ForTypicallySecureUsers);
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

  // Last fallback event is now a day old. HFM should be enabled now.
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
