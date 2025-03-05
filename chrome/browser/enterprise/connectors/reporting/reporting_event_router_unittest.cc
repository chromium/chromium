// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router.h"

#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/mock_realtime_reporting_client.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

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
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken("fake-token");

    enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
        ->SetTestingFactory(
            profile_, base::BindRepeating([](content::BrowserContext* context) {
              return std::unique_ptr<KeyedService>(
                  new enterprise_connectors::RealtimeReportingClient(context));
            }));
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile_)
        ->SetBrowserCloudPolicyClientForTesting(client_.get());

    reporting_event_router_ = std::make_unique<ReportingEventRouter>(profile_);

    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile_)
        ->SetIdentityManagerForTesting(
            identity_test_environment_.identity_manager());
    identity_test_environment_.MakePrimaryAccountAvailable(
        profile_->GetProfileUserName(), signin::ConsentLevel::kSignin);
  }

  void TearDown() override {
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile_)
        ->SetBrowserCloudPolicyClientForTesting(nullptr);
  }

  std::string GetProfileIdentifier() const {
    return profile_->GetPath().AsUTF8Unsafe();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<ReportingEventRouter> reporting_event_router_;
  signin::IdentityTestEnvironment identity_test_environment_;
};

TEST_F(ReportingEventRouterTest, CheckEventEnabledReturnsFalse) {
  test::SetOnSecurityEventReporting(profile_->GetPrefs(), /*enabled=*/false,
                                    /*enabled_event_names=*/{},
                                    /*enabled_opt_in_events=*/{});

  EXPECT_FALSE(reporting_event_router_->IsEventEnabled(kKeyPasswordReuseEvent));
}

TEST_F(ReportingEventRouterTest, CheckEventEnabledReturnsTrue) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordReuseEvent},
      /*enabled_opt_in_events=*/{});

  EXPECT_TRUE(reporting_event_router_->IsEventEnabled(kKeyPasswordReuseEvent));
}

TEST_F(ReportingEventRouterTest, TestOnLoginEvent) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectLoginEvent("https://www.example.com/", false, "",
                             profile_->GetProfileUserName(),
                             GetProfileIdentifier(), u"*****");

  reporting_event_router_->OnLoginEvent(GURL("https://www.example.com/"),
                                        url::SchemeHostPort().IsValid(),
                                        url::SchemeHostPort(), u"Fakeuser");
}

TEST_F(ReportingEventRouterTest, TestOnLoginEventNoMatchingUrlPattern) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"notexample.com"}}});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectNoReport();

  reporting_event_router_->OnLoginEvent(
      GURL("https://www.example.com/"), url::SchemeHostPort().IsValid(),
      url::SchemeHostPort(), u"login-username");
}

TEST_F(ReportingEventRouterTest, TestOnLoginEventWithEmailAsLoginUsername) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectLoginEvent("https://www.example.com/", false, "",
                             profile_->GetProfileUserName(),
                             GetProfileIdentifier(), u"*****@example.com");

  reporting_event_router_->OnLoginEvent(
      GURL("https://www.example.com/"), url::SchemeHostPort().IsValid(),
      url::SchemeHostPort(), u"Fakeuser@example.com");
}

TEST_F(ReportingEventRouterTest, TestOnLoginEventFederated) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectLoginEvent(
      "https://www.example.com/", true, "https://www.google.com",
      profile_->GetProfileUserName(), GetProfileIdentifier(), u"*****");

  url::SchemeHostPort federated_origin =
      url::SchemeHostPort(GURL("https://www.google.com"));

  reporting_event_router_->OnLoginEvent(GURL("https://www.example.com/"),
                                        federated_origin.IsValid(),
                                        federated_origin, u"Fakeuser");
}

TEST_F(ReportingEventRouterTest, TestOnPasswordBreach) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyPasswordBreachEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectPasswordBreachEvent(
      "SAFETY_CHECK",
      {
          {"https://first.example.com/", u"*****"},
          {"https://second.example.com/", u"*****@gmail.com"},
      },
      profile_->GetProfileUserName(), GetProfileIdentifier());

  reporting_event_router_->OnPasswordBreach(
      "SAFETY_CHECK",
      {
          {GURL("https://first.example.com"), u"first_user_name"},
          {GURL("https://second.example.com"), u"second_user_name@gmail.com"},
      });
}

TEST_F(ReportingEventRouterTest, TestOnPasswordBreachNoMatchingUrlPattern) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{kKeyPasswordBreachEvent, {"notexample.com"}}});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectNoReport();

  reporting_event_router_->OnPasswordBreach(
      "SAFETY_CHECK",
      {
          {GURL("https://first.example.com"), u"first_user_name"},
          {GURL("https://second.example.com"), u"second_user_name"},
      });
}

TEST_F(ReportingEventRouterTest,
       TestOnPasswordBreachPartiallyMatchingUrlPatterns) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{kKeyPasswordBreachEvent, {"secondexample.com"}}});

  // The event is only enabled on secondexample.com, so expect only the
  // information related to that origin to be reported.
  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectPasswordBreachEvent(
      "SAFETY_CHECK",
      {
          {"https://secondexample.com/", u"*****"},
      },
      profile_->GetProfileUserName(), GetProfileIdentifier());

  reporting_event_router_->OnPasswordBreach(
      "SAFETY_CHECK",
      {
          {GURL("https://firstexample.com"), u"first_user_name"},
          {GURL("https://secondexample.com"), u"second_user_name"},
      });
}

}  // namespace enterprise_connectors
