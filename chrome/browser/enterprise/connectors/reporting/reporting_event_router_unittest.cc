// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router.h"

#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/mock_realtime_reporting_client.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kFakeProfileUsername[] = "Fakeuser";

}  // namespace

class ReportingEventRouterTest : public testing::Test {
 public:
  ReportingEventRouterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeProfileUsername);
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));

    RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&test::MockRealtimeReportingClient::
                                          CreateMockRealtimeReportingClient));
    reportingEventRouter_ = std::make_unique<ReportingEventRouter>(profile_);

    mockRealtimeReportingClient_ =
        static_cast<test::MockRealtimeReportingClient*>(
            RealtimeReportingClientFactory::GetForProfile(profile_));
    mockRealtimeReportingClient_->SetProfileUserNameForTesting(
        kFakeProfileUsername);
  }

  void TearDown() override {
    mockRealtimeReportingClient_->SetBrowserCloudPolicyClientForTesting(
        nullptr);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<test::MockRealtimeReportingClient> mockRealtimeReportingClient_;
  std::unique_ptr<ReportingEventRouter> reportingEventRouter_;
};

TEST_F(ReportingEventRouterTest, CheckEventEnabledReturnsFalse) {
  test::SetOnSecurityEventReporting(profile_->GetPrefs(), /*enabled=*/false,
                                    /*enabled_event_names=*/{},
                                    /*enabled_opt_in_events=*/{});

  // Set a mock cloud policy client in the router.
  client_ = std::make_unique<policy::MockCloudPolicyClient>();
  client_->SetDMToken("fake-token");
  mockRealtimeReportingClient_->SetBrowserCloudPolicyClientForTesting(
      client_.get());

  EXPECT_FALSE(reportingEventRouter_->IsEventEnabled(kKeyPasswordReuseEvent));
}

TEST_F(ReportingEventRouterTest, CheckEventEnabledReturnsTrue) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordReuseEvent},
      /*enabled_opt_in_events=*/{});

  // Set a mock cloud policy client in the router.
  client_ = std::make_unique<policy::MockCloudPolicyClient>();
  client_->SetDMToken("fake-token");
  mockRealtimeReportingClient_->SetBrowserCloudPolicyClientForTesting(
      client_.get());

  EXPECT_TRUE(reportingEventRouter_->IsEventEnabled(kKeyPasswordReuseEvent));
}

}  // namespace enterprise_connectors
