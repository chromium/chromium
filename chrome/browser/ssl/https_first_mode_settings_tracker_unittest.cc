// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(profile());
  ASSERT_TRUE(engagement_service);

  StatefulSSLHostStateDelegate* state =
      StatefulSSLHostStateDelegateFactory::GetForProfile(profile());
  ASSERT_TRUE(state);

  GURL https_url("https://example.com");
  GURL http_url("http://example.com");

  // HFM should initially be disabled on this site by default.
  service->MaybeEnableHttpsFirstModeForUrl(profile(), https_url);
  EXPECT_FALSE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 0);

  // Increase the score, should now enable HFM.
  engagement_service->ResetBaseScoreForURL(https_url, 90);
  service->MaybeEnableHttpsFirstModeForUrl(profile(), https_url);
  EXPECT_TRUE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kEnabled, 1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kDisabled, 0);

  // Decrease the score, but only slightly. This shouldn't result in HFM being
  // disabled.
  engagement_service->ResetBaseScoreForURL(https_url, 85);
  service->MaybeEnableHttpsFirstModeForUrl(profile(), https_url);
  EXPECT_TRUE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kEnabled, 1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kDisabled, 0);

  // Decrease the score further. This should result in HFM being disabled.
  engagement_service->ResetBaseScoreForURL(https_url, 25);
  service->MaybeEnableHttpsFirstModeForUrl(profile(), https_url);
  EXPECT_FALSE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 2);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kEnabled, 1);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kDisabled, 1);

  // Increase the score again and re-enable HFM.
  engagement_service->ResetBaseScoreForURL(https_url, 90);
  service->MaybeEnableHttpsFirstModeForUrl(profile(), https_url);
  EXPECT_TRUE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 3);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kEnabled, 2);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kDisabled, 1);

  // Also increase the HTTP score. This should disable HFM even though the
  // HTTPS score is still high.
  engagement_service->ResetBaseScoreForURL(http_url, 20);
  service->MaybeEnableHttpsFirstModeForUrl(profile(), https_url);
  EXPECT_FALSE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 4);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kEnabled, 2);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kDisabled, 2);

  // Set HTTP score to max and set HTTPS score to zero. This simulates the user
  // spending their time on the HTTP URL and the HTTPS score decaying over time.
  engagement_service->ResetBaseScoreForURL(https_url, 0);
  engagement_service->ResetBaseScoreForURL(http_url, 100);
  service->MaybeEnableHttpsFirstModeForUrl(profile(), https_url);
  EXPECT_FALSE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));
  histograms.ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 4);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kEnabled, 2);
  histograms.ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                               SiteEngagementHeuristicState::kDisabled, 2);

  service->Shutdown();
}
