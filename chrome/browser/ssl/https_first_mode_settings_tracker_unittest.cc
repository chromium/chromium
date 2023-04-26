// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(profile());
  ASSERT_TRUE(engagement_service);

  StatefulSSLHostStateDelegate* state =
      StatefulSSLHostStateDelegateFactory::GetForProfile(profile());
  ASSERT_TRUE(state);

  GURL url("https://example.com");

  // HFM should initially be disabled on this site by default.
  service->MaybeEnableHttpsFirstModeForUrl(profile(), url);
  EXPECT_FALSE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));

  // Increase the score, should now enable HFM.
  engagement_service->ResetBaseScoreForURL(url, 90);
  service->MaybeEnableHttpsFirstModeForUrl(profile(), url);
  EXPECT_TRUE(state->IsHttpsEnforcedForHost(
      "example.com", profile()->GetDefaultStoragePartition()));

  service->Shutdown();
}
