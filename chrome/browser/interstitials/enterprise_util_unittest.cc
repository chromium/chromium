// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/enterprise_util.h"

#include <string>

#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class InterstitialEnterpriseUtilTest : public testing::Test {
 public:
  InterstitialEnterpriseUtilTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
  }

  void SetUp() override {
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));
  }

  void EnableReportingPolicy(Profile* profile) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            profile, base::BindRepeating(
                         &safe_browsing::BuildSafeBrowsingPrivateEventRouter));
    enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
        ->SetTestingFactory(
            profile,
            base::BindRepeating(&safe_browsing::BuildRealtimeReportingClient));
    enterprise_connectors::test::SetOnSecurityEventReporting(
        profile->GetPrefs(), /*enabled=*/true, /*enabled_event_names=*/{},
        /*enabled_opt_in_events=*/{});

    // Set a mock cloud policy client in the router.
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken("fake-token");
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile)
        ->SetBrowserCloudPolicyClientForTesting(client_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  TestingProfileManager profile_manager_;
  content::TestWebContentsFactory web_contents_factory_;
};

TEST_F(InterstitialEnterpriseUtilTest, RouterEventDisabledInIncognitoMode) {
  Profile* incognito_profile =
      profile_manager_.CreateTestingProfile("testing_profile")
          ->GetPrimaryOTRProfile(
              /*create_if_needed=*/true);
  EnableReportingPolicy(incognito_profile);
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);
  MaybeTriggerSecurityInterstitialShownEvent(
      web_contents_factory_.CreateWebContents(incognito_profile),
      GURL("https://phishing.com/"), "reason",
      /*net_error_code=*/0);
}

TEST_F(InterstitialEnterpriseUtilTest, RouterEventEnabledInGuestMode) {
  Profile* guest_profile =
      profile_manager_.CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true);
  EnableReportingPolicy(guest_profile);
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(1);
  MaybeTriggerSecurityInterstitialShownEvent(
      web_contents_factory_.CreateWebContents(guest_profile),
      GURL("https://phishing.com/"), "reason",
      /*net_error_code=*/0);
}
