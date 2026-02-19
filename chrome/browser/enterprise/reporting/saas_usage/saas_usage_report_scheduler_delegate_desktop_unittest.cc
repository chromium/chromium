// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_report_scheduler_delegate_desktop.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/mock_realtime_reporting_client.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

class SaasUsageReportSchedulerDelegateDesktopTest : public testing::Test {
 public:
  SaasUsageReportSchedulerDelegateDesktopTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  TestingProfile* CreateProfile(const std::string& name, bool has_client) {
    TestingProfile::TestingFactories profile_testing_factories;
    if (has_client) {
      profile_testing_factories.push_back(
          {enterprise_connectors::RealtimeReportingClientFactory::GetInstance(),
           base::BindRepeating(
               &enterprise_connectors::test::MockRealtimeReportingClient::
                   CreateMockRealtimeReportingClient)});
    }
    return profile_manager_.CreateTestingProfile(
        name, std::move(profile_testing_factories));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST_F(SaasUsageReportSchedulerDelegateDesktopTest, IsReady_NoProfiles) {
  SaasUsageReportSchedulerDelegateDesktop delegate;
  EXPECT_FALSE(delegate.IsReady());
}

TEST_F(SaasUsageReportSchedulerDelegateDesktopTest,
       IsReady_ProfileWithoutClient) {
  SaasUsageReportSchedulerDelegateDesktop delegate;
  CreateProfile("p1", /*has_client=*/false);
  EXPECT_FALSE(delegate.IsReady());
}

TEST_F(SaasUsageReportSchedulerDelegateDesktopTest, IsReady_ProfileWithClient) {
  SaasUsageReportSchedulerDelegateDesktop delegate;
  CreateProfile("p1", /*has_client=*/true);
  EXPECT_TRUE(delegate.IsReady());
}

TEST_F(SaasUsageReportSchedulerDelegateDesktopTest, IsReady_MixedProfiles) {
  SaasUsageReportSchedulerDelegateDesktop delegate;
  CreateProfile("p1", /*has_client=*/false);
  EXPECT_FALSE(delegate.IsReady());
  CreateProfile("p2", /*has_client=*/true);
  EXPECT_TRUE(delegate.IsReady());
}

TEST_F(SaasUsageReportSchedulerDelegateDesktopTest,
       CallbackCalledWhenProfileWithClientAdded) {
  SaasUsageReportSchedulerDelegateDesktop delegate;
  base::MockCallback<base::RepeatingClosure> callback;
  delegate.SetReadyStateChangedCallback(callback.Get());

  EXPECT_CALL(callback, Run()).Times(1);
  CreateProfile("p1", /*has_client=*/true);
}

TEST_F(SaasUsageReportSchedulerDelegateDesktopTest,
       CallbackNotCalledWhenProfileWithoutClientAdded) {
  SaasUsageReportSchedulerDelegateDesktop delegate;
  base::MockCallback<base::RepeatingClosure> callback;
  delegate.SetReadyStateChangedCallback(callback.Get());

  EXPECT_CALL(callback, Run()).Times(0);
  CreateProfile("p1", /*has_client=*/false);
}

TEST_F(SaasUsageReportSchedulerDelegateDesktopTest,
       NotifiesOnlyWhenReadyStateChanges) {
  SaasUsageReportSchedulerDelegateDesktop delegate;
  base::MockCallback<base::RepeatingClosure> callback;
  delegate.SetReadyStateChangedCallback(callback.Get());

  // Callback should be called when the first profile with a client is added.
  EXPECT_CALL(callback, Run()).Times(1);
  CreateProfile("p1", /*has_client=*/true);
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Callback should not be called again if the delegate is already ready.
  EXPECT_CALL(callback, Run()).Times(0);
  CreateProfile("p2", /*has_client=*/true);
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Callback should not be called when there are still profiles with client.
  EXPECT_CALL(callback, Run()).Times(0);
  profile_manager_.DeleteTestingProfile("p2");
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Callback should be called when the last profile with a client is removed.
  EXPECT_CALL(callback, Run()).Times(1);
  profile_manager_.DeleteTestingProfile("p1");
  testing::Mock::VerifyAndClearExpectations(&callback);
}

}  // namespace enterprise_reporting
