// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_event_router.h"

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
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

namespace enterprise_connectors {

namespace {

// Alias to reduce verbosity when using TriggeredRuleInfo.
using TriggeredRuleInfo = ::chrome::cros::reporting::proto::TriggeredRuleInfo;
// Alias to reduce verbosity when using the repeated ReferrerChainEntry field.
using ReferrerChain =
    google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>;
// Alias to reduce verbosity when using UrlInfo.
using UrlInfo = ::chrome::cros::reporting::proto::UrlInfo;

constexpr char kFakeProfileUsername[] = "Fakeuser";

}  // namespace

class ReportingEventRouterTest : public testing::TestWithParam<bool> {
 public:
  ReportingEventRouterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    if (use_proto_format()) {
      scoped_feature_list_.InitAndEnableFeature(
          policy::kUploadRealtimeReportingEventsUsingProto);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          policy::kUploadRealtimeReportingEventsUsingProto);
    }

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

    reporting_event_router_ = std::make_unique<ReportingEventRouter>(
        RealtimeReportingClientFactory::GetForProfile(profile_));

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

  bool use_proto_format() { return GetParam(); }

  std::string GetProfileIdentifier() const {
    return profile_->GetPath().AsUTF8Unsafe();
  }

  void EnableEnhancedFieldsForSecOps() {
    scoped_feature_list_.Reset();
    if (use_proto_format()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{policy::
                                    kUploadRealtimeReportingEventsUsingProto,
                                safe_browsing::kEnhancedFieldsForSecOps},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          safe_browsing::kEnhancedFieldsForSecOps);
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<ReportingEventRouter> reporting_event_router_;
  signin::IdentityTestEnvironment identity_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ReportingEventRouterTest, CheckEventEnabledReturnsFalse) {
  test::SetOnSecurityEventReporting(profile_->GetPrefs(), /*enabled=*/false,
                                    /*enabled_event_names=*/{},
                                    /*enabled_opt_in_events=*/{});

  EXPECT_FALSE(reporting_event_router_->IsEventEnabled(kKeyPasswordReuseEvent));
}

TEST_P(ReportingEventRouterTest, CheckEventEnabledReturnsTrue) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordReuseEvent},
      /*enabled_opt_in_events=*/{});

  EXPECT_TRUE(reporting_event_router_->IsEventEnabled(kKeyPasswordReuseEvent));
}

TEST_P(ReportingEventRouterTest, TestOnLoginEvent) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  chrome::cros::reporting::proto::LoginEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://www.example.com/");
    expected_event.set_is_federated(false);
    expected_event.set_profile_user_name(profile_->GetProfileUserName());
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_login_user_name("*****");

    validator.ExpectLoginEvent(std::move(expected_event));
  } else {
    validator.ExpectLoginEvent("https://www.example.com/", false, "",
                               profile_->GetProfileUserName(),
                               GetProfileIdentifier(), u"*****");
  }

  reporting_event_router_->OnLoginEvent(GURL("https://www.example.com/"),
                                        url::SchemeHostPort().IsValid(),
                                        url::SchemeHostPort(), u"Fakeuser");
}

TEST_P(ReportingEventRouterTest, TestOnLoginEventNoMatchingUrlPattern) {
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

TEST_P(ReportingEventRouterTest, TestOnLoginEventWithEmailAsLoginUsername) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  chrome::cros::reporting::proto::LoginEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://www.example.com/");
    expected_event.set_is_federated(false);
    expected_event.set_profile_user_name(profile_->GetProfileUserName());
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_login_user_name("*****@example.com");

    validator.ExpectLoginEvent(std::move(expected_event));
  } else {
    validator.ExpectLoginEvent("https://www.example.com/", false, "",
                               profile_->GetProfileUserName(),
                               GetProfileIdentifier(), u"*****@example.com");
  }

  reporting_event_router_->OnLoginEvent(
      GURL("https://www.example.com/"), url::SchemeHostPort().IsValid(),
      url::SchemeHostPort(), u"Fakeuser@example.com");
}

TEST_P(ReportingEventRouterTest, TestOnLoginEventFederated) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  chrome::cros::reporting::proto::LoginEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://www.example.com/");
    expected_event.set_is_federated(true);
    expected_event.set_federated_origin("https://www.google.com");
    expected_event.set_profile_user_name(profile_->GetProfileUserName());
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_login_user_name("*****");

    validator.ExpectLoginEvent(std::move(expected_event));
  } else {
    validator.ExpectLoginEvent(
        "https://www.example.com/", true, "https://www.google.com",
        profile_->GetProfileUserName(), GetProfileIdentifier(), u"*****");
  }

  url::SchemeHostPort federated_origin =
      url::SchemeHostPort(GURL("https://www.google.com"));

  reporting_event_router_->OnLoginEvent(GURL("https://www.example.com/"),
                                        federated_origin.IsValid(),
                                        federated_origin, u"Fakeuser");
}

TEST_P(ReportingEventRouterTest, TestOnPasswordBreach) {
  // TODO(crbug.com/396436374): Migrate password breach event to proto format.
  if (use_proto_format()) {
    return;
  }
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

TEST_P(ReportingEventRouterTest, TestOnPasswordBreachNoMatchingUrlPattern) {
  // TODO(crbug.com/396436374): Migrate password breach event to proto format.
  if (use_proto_format()) {
    return;
  }
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

TEST_P(ReportingEventRouterTest,
       TestOnPasswordBreachPartiallyMatchingUrlPatterns) {
  // TODO(crbug.com/396436374): Migrate password breach event to proto format.
  if (use_proto_format()) {
    return;
  }
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

TEST_P(ReportingEventRouterTest, TestOnUrlFilteringInterstitial_Blocked) {
  // TODO(crbug.com/396439420): Migrate url filtering event to proto format.
  if (use_proto_format()) {
    return;
  }
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_BLOCKED);
  expected_event.set_profile_user_name(profile_->GetProfileUserName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = test::MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::BLOCK, /*has_watermark=*/false);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectURLFilteringInterstitialEventWithReferrers(expected_event);

  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id("123");
  matched_url_navigation_rule->set_rule_name("test rule name");
  matched_url_navigation_rule->set_matched_url_category("test rule category");
  ReferrerChain referrer_chain;

  reporting_event_router_->OnUrlFilteringInterstitial(
      GURL("https://filteredurl.com"), "ENTERPRISE_BLOCKED_SEEN", response,
      referrer_chain);
}

TEST_P(ReportingEventRouterTest, TestOnUrlFilteringInterstitial_Warned) {
  // TODO(crbug.com/396439420): Migrate url filtering event to proto format.
  if (use_proto_format()) {
    return;
  }
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_WARNED);
  expected_event.set_profile_user_name(profile_->GetProfileUserName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = test::MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::WARN, /*has_watermark=*/true);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectURLFilteringInterstitialEventWithReferrers(expected_event);

  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::WARN);
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id("123");
  matched_url_navigation_rule->set_rule_name("test rule name");
  matched_url_navigation_rule->set_matched_url_category("test rule category");
  matched_url_navigation_rule->mutable_watermark_message()
      ->set_watermark_message("watermark message");
  ReferrerChain referrer_chain;

  reporting_event_router_->OnUrlFilteringInterstitial(
      GURL("https://filteredurl.com"), "ENTERPRISE_WARNED_SEEN", response,
      referrer_chain);
}

TEST_P(ReportingEventRouterTest, TestOnUrlFilteringInterstitial_Bypassed) {
  // TODO(crbug.com/396439420): Migrate url filtering event to proto format.
  if (use_proto_format()) {
    return;
  }
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_BYPASSED);
  expected_event.set_profile_user_name(profile_->GetProfileUserName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = test::MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::WARN, /*has_watermark=*/true);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectURLFilteringInterstitialEventWithReferrers(expected_event);

  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::WARN);
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id("123");
  matched_url_navigation_rule->set_rule_name("test rule name");
  matched_url_navigation_rule->set_matched_url_category("test rule category");
  matched_url_navigation_rule->mutable_watermark_message()
      ->set_watermark_message("confidential");
  ReferrerChain referrer_chain;

  reporting_event_router_->OnUrlFilteringInterstitial(
      GURL("https://filteredurl.com"), "ENTERPRISE_WARNED_BYPASS", response,
      referrer_chain);
}

TEST_P(ReportingEventRouterTest,
       TestOnUrlFilteringInterstitial_WatermarkAudit) {
  // TODO(crbug.com/396439420): Migrate url filtering event to proto format.
  if (use_proto_format()) {
    return;
  }
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_ALLOWED);
  expected_event.set_profile_user_name(profile_->GetProfileUserName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = test::MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::ACTION_UNKNOWN, /*has_watermark=*/true);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectURLFilteringInterstitialEventWithReferrers(expected_event);

  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id("123");
  matched_url_navigation_rule->set_rule_name("test rule name");
  matched_url_navigation_rule->set_matched_url_category("test rule category");
  matched_url_navigation_rule->mutable_watermark_message()
      ->set_watermark_message("confidential");
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());

  reporting_event_router_->OnUrlFilteringInterstitial(
      GURL("https://filteredurl.com"), "", response, referrer_chain);
}

TEST_P(ReportingEventRouterTest, TestInterstitialShownWarned) {
  // TODO(crbug.com/396437371): Migrate secutiry interstital event to proto
  // format.
  if (use_proto_format()) {
    return;
  }
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectSecurityInterstitialEventWithReferrers(
      "https://phishing.com/", "PHISHING", profile_->GetProfileUserName(),
      GetProfileIdentifier(), "EVENT_RESULT_WARNED", false, 0,
      test::MakeUrlInfoReferrer());
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSecurityInterstitialShown(
      GURL("https://phishing.com/"), "PHISHING", 0, false, referrer_chain);
}

TEST_P(ReportingEventRouterTest, TestInterstitialShownBlocked) {
  // TODO(crbug.com/396437371): Migrate secutiry interstital event to proto
  // format.
  if (use_proto_format()) {
    return;
  }
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectSecurityInterstitialEventWithReferrers(
      "https://phishing.com/", "PHISHING", profile_->GetProfileUserName(),
      GetProfileIdentifier(), "EVENT_RESULT_BLOCKED", false, 0,
      test::MakeUrlInfoReferrer());
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSecurityInterstitialShown(
      GURL("https://phishing.com/"), "PHISHING", 0, true, referrer_chain);
}

TEST_P(ReportingEventRouterTest, TestInterstitialProceeded) {
  // TODO(crbug.com/396437371): Migrate secutiry interstital event to proto
  // format.
  if (use_proto_format()) {
    return;
  }
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectSecurityInterstitialEventWithReferrers(
      "https://phishing.com/", "PHISHING", profile_->GetProfileUserName(),
      GetProfileIdentifier(), "EVENT_RESULT_BYPASSED", true, 0,
      test::MakeUrlInfoReferrer());
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSecurityInterstitialProceeded(
      GURL("https://phishing.com/"), "PHISHING", 0, referrer_chain);
}

TEST_P(ReportingEventRouterTest, TestPasswordReuseWarned) {
  // TODO(crbug.com/396437152): Migrate password reuse event to proto format.
  if (use_proto_format()) {
    return;
  }
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordReuseEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectPasswordReuseEvent(
      "https://phishing.com/", "user_name_1", true, "EVENT_RESULT_WARNED",
      profile_->GetProfileUserName(), GetProfileIdentifier());
  reporting_event_router_->OnPasswordReuse(
      GURL("https://phishing.com/"), "user_name_1", /*is_phishing_url*/ true,
      /*warning_shown*/ true);
}

TEST_P(ReportingEventRouterTest, TestPasswordReuseAllowed) {
  // TODO(crbug.com/396437152): Migrate password reuse event to proto format.
  if (use_proto_format()) {
    return;
  }
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordReuseEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectPasswordReuseEvent(
      "https://phishing.com/", "user_name_1", true, "EVENT_RESULT_ALLOWED",
      profile_->GetProfileUserName(), GetProfileIdentifier());
  reporting_event_router_->OnPasswordReuse(
      GURL("https://phishing.com/"), "user_name_1", /*is_phishing_url*/ true,
      /*warning_shown*/ false);
}

TEST_P(ReportingEventRouterTest, TestPasswordChanged) {
  // TODO(crbug.com//396437063): Migrate password changed event to proto format.
  if (use_proto_format()) {
    return;
  }
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordChangedEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectPassowrdChangedEvent(
      "user_name_1", profile_->GetProfileUserName(), GetProfileIdentifier());
  reporting_event_router_->OnPasswordChanged("user_name_1");
}

INSTANTIATE_TEST_SUITE_P(, ReportingEventRouterTest, ::testing::Bool());

}  // namespace enterprise_connectors
