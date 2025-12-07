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
#include "components/download/public/common/download_danger_type.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/features.h"
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
constexpr char kFakeActiveUserEmail[] = "active_user@example.com";

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
const std::set<std::string>* ZipMimeType() {
  static std::set<std::string> set = {"application/zip"};
  return &set;
}

const std::vector<std::string>& GetFakeFrameUrlChain() {
  static const std::vector<std::string> kFrameUrls = {"https://frame1.com/",
                                                      "https://frame2.com/"};
  return kFrameUrls;
}

google::protobuf::RepeatedPtrField<std::string> CreateFakeFrameUrlChainProto() {
  google::protobuf::RepeatedPtrField<std::string> chain;
  for (const auto& url : GetFakeFrameUrlChain()) {
    *chain.Add() = url;
  }
  return chain;
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

}  // namespace

// A RealtimeReportingClient that always returns a specific account as the
// active account in the content area. This is used to test the active user
// detection feature.
class RealtimeReportingClientFakeActiveGaia : public RealtimeReportingClient {
 public:
  explicit RealtimeReportingClientFakeActiveGaia(
      content::BrowserContext* context)
      : RealtimeReportingClient(context) {}
  ~RealtimeReportingClientFakeActiveGaia() override = default;
  RealtimeReportingClientFakeActiveGaia(
      const RealtimeReportingClientFakeActiveGaia&) = delete;
  RealtimeReportingClientFakeActiveGaia& operator=(
      const RealtimeReportingClientFakeActiveGaia&) = delete;

  std::string GetContentAreaAccountEmail(const GURL& url) override {
    return kFakeActiveUserEmail;
  }
};

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

    RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              new RealtimeReportingClientFakeActiveGaia(context));
        }));
    RealtimeReportingClientFactory::GetForProfile(profile_)
        ->SetBrowserCloudPolicyClientForTesting(client_.get());

    reporting_event_router_ = std::make_unique<ReportingEventRouter>(
        RealtimeReportingClientFactory::GetForProfile(profile_));

    RealtimeReportingClientFactory::GetForProfile(profile_)
        ->SetIdentityManagerForTesting(
            identity_test_environment_.identity_manager());
    identity_test_environment_.MakePrimaryAccountAvailable(
        profile_->GetProfileUserName(), signin::ConsentLevel::kSignin);
  }

  void TearDown() override {
    RealtimeReportingClientFactory::GetForProfile(profile_)
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

  void EnableEnhancedFieldsForSecOpsAndActiveUserDetection() {
    scoped_feature_list_.Reset();
    if (use_proto_format()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {policy::kUploadRealtimeReportingEventsUsingProto,
           safe_browsing::kEnhancedFieldsForSecOps,
           enterprise_connectors::kEnterpriseActiveUserDetection},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {safe_browsing::kEnhancedFieldsForSecOps,
           enterprise_connectors::kEnterpriseActiveUserDetection},
          /*disabled_features=*/{});
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
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
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
  run_loop.Run();
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
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
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
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestOnLoginEventFederated) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
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
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestOnPasswordBreach) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyPasswordBreachEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::PasswordBreachEvent expected_event;
  if (use_proto_format()) {
    chrome::cros::reporting::proto::PasswordBreachEvent::Identity identity_1;
    identity_1.set_url("https://first.example.com/");
    identity_1.set_username("*****");
    chrome::cros::reporting::proto::PasswordBreachEvent::Identity identity_2;
    identity_2.set_url("https://second.example.com/");
    identity_2.set_username("*****@gmail.com");
    *expected_event.add_identities() = identity_1;
    *expected_event.add_identities() = identity_2;
    expected_event.set_trigger(
        chrome::cros::reporting::proto::PasswordBreachEvent::SAFETY_CHECK);
    expected_event.set_profile_user_name(profile_->GetProfileUserName());
    expected_event.set_profile_identifier(GetProfileIdentifier());

    validator.ExpectPasswordBreachEvent(std::move(expected_event));
  } else {
    validator.ExpectPasswordBreachEvent(
        "SAFETY_CHECK",
        {
            {"https://first.example.com/", u"*****"},
            {"https://second.example.com/", u"*****@gmail.com"},
        },
        profile_->GetProfileUserName(), GetProfileIdentifier());
  }

  reporting_event_router_->OnPasswordBreach(
      "SAFETY_CHECK",
      {
          {GURL("https://first.example.com"), u"first_user_name"},
          {GURL("https://second.example.com"), u"second_user_name@gmail.com"},
      });
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestOnPasswordBreachNoMatchingUrlPattern) {
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
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{kKeyPasswordBreachEvent, {"secondexample.com"}}});

  // The event is only enabled on secondexample.com, so expect only the
  // information related to that origin to be reported.
  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::PasswordBreachEvent expected_event;

  if (use_proto_format()) {
    chrome::cros::reporting::proto::PasswordBreachEvent::Identity identity;
    identity.set_url("https://secondexample.com/");
    identity.set_username("*****");
    *expected_event.add_identities() = identity;
    expected_event.set_trigger(
        chrome::cros::reporting::proto::PasswordBreachEvent::SAFETY_CHECK);
    expected_event.set_profile_user_name(profile_->GetProfileUserName());
    expected_event.set_profile_identifier(GetProfileIdentifier());

    validator.ExpectPasswordBreachEvent(std::move(expected_event));
  } else {
    validator.ExpectPasswordBreachEvent(
        "SAFETY_CHECK",
        {
            {"https://secondexample.com/", u"*****"},
        },
        profile_->GetProfileUserName(), GetProfileIdentifier());
  }

  reporting_event_router_->OnPasswordBreach(
      "SAFETY_CHECK",
      {
          {GURL("https://firstexample.com"), u"first_user_name"},
          {GURL("https://secondexample.com"), u"second_user_name"},
      });
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestOnUrlFilteringInterstitial_Blocked) {
  EnableEnhancedFieldsForSecOpsAndActiveUserDetection();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;

  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_BLOCKED);
  expected_event.set_threat_type(
      chrome::cros::reporting::proto::UrlFilteringInterstitialEvent::
          ENTERPRISE_BLOCKED_SEEN);
  expected_event.set_profile_user_name(profile_->GetProfileUserName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = test::MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::BLOCK, /*has_watermark=*/false);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();
  expected_event.set_web_app_signed_in_account(kFakeActiveUserEmail);

  if (use_proto_format()) {
    validator.ExpectProtoBasedUrlFilteringInterstitialEvent(expected_event);
  } else {
    validator.ExpectURLFilteringInterstitialEventWithReferrers(expected_event);
  }

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
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnUrlFilteringInterstitial(
      GURL("https://filteredurl.com"), "ENTERPRISE_BLOCKED_SEEN", response,
      referrer_chain);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestOnUrlFilteringInterstitial_Warned) {
  EnableEnhancedFieldsForSecOpsAndActiveUserDetection();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;

  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_WARNED);
  expected_event.set_threat_type(
      chrome::cros::reporting::proto::UrlFilteringInterstitialEvent::
          ENTERPRISE_WARNED_SEEN);
  expected_event.set_profile_user_name(profile_->GetProfileUserName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = test::MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::WARN, /*has_watermark=*/true);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();
  expected_event.set_web_app_signed_in_account(kFakeActiveUserEmail);

  if (use_proto_format()) {
    validator.ExpectProtoBasedUrlFilteringInterstitialEvent(expected_event);
  } else {
    validator.ExpectURLFilteringInterstitialEventWithReferrers(expected_event);
  }

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
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnUrlFilteringInterstitial(
      GURL("https://filteredurl.com"), "ENTERPRISE_WARNED_SEEN", response,
      referrer_chain);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestOnUrlFilteringInterstitial_Bypassed) {
  EnableEnhancedFieldsForSecOpsAndActiveUserDetection();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;

  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_BYPASSED);
  expected_event.set_clicked_through(true);
  expected_event.set_threat_type(
      chrome::cros::reporting::proto::UrlFilteringInterstitialEvent::
          ENTERPRISE_WARNED_BYPASS);
  expected_event.set_profile_user_name(profile_->GetProfileUserName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = test::MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::WARN, /*has_watermark=*/true);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();
  expected_event.set_web_app_signed_in_account(kFakeActiveUserEmail);

  if (use_proto_format()) {
    validator.ExpectProtoBasedUrlFilteringInterstitialEvent(expected_event);
  } else {
    validator.ExpectURLFilteringInterstitialEventWithReferrers(expected_event);
  }

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
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnUrlFilteringInterstitial(
      GURL("https://filteredurl.com"), "ENTERPRISE_WARNED_BYPASS", response,
      referrer_chain);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest,
       TestOnUrlFilteringInterstitial_WatermarkAudit) {
  EnableEnhancedFieldsForSecOpsAndActiveUserDetection();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;

  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_ALLOWED);
  expected_event.set_threat_type(
      chrome::cros::reporting::proto::UrlFilteringInterstitialEvent::
          UNKNOWN_INTERSTITIAL_THREAT_TYPE);
  expected_event.set_profile_user_name(profile_->GetProfileUserName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = test::MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::ACTION_UNKNOWN, /*has_watermark=*/true);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();
  expected_event.set_web_app_signed_in_account(kFakeActiveUserEmail);

  if (use_proto_format()) {
    validator.ExpectProtoBasedUrlFilteringInterstitialEvent(expected_event);
  } else {
    validator.ExpectURLFilteringInterstitialEventWithReferrers(expected_event);
  }

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
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestInterstitialShownWarned) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://phishing.com/");
    expected_event.set_reason(chrome::cros::reporting::proto::
                                  SafeBrowsingInterstitialEvent::PHISHING);
    expected_event.set_profile_user_name(profile_->GetProfileUserName());
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EVENT_RESULT_WARNED);
    expected_event.set_clicked_through(false);
    expected_event.set_net_error_code(0);
    expected_event.mutable_referrers()->Add(test::MakeUrlInfoReferrer());

    validator.ExpectSecurityInterstitialEvent(std::move(expected_event));
  } else {
    validator.ExpectSecurityInterstitialEventWithReferrers(
        "https://phishing.com/", "PHISHING", profile_->GetProfileUserName(),
        GetProfileIdentifier(), "EVENT_RESULT_WARNED", false, 0,
        test::MakeUrlInfoReferrer());
  }

  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSecurityInterstitialShown(
      GURL("https://phishing.com/"), "PHISHING", 0, false, referrer_chain);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestInterstitialShownBlocked) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://phishing.com/");
    expected_event.set_reason(chrome::cros::reporting::proto::
                                  SafeBrowsingInterstitialEvent::PHISHING);
    expected_event.set_profile_user_name(profile_->GetProfileUserName());
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EVENT_RESULT_BLOCKED);
    expected_event.set_clicked_through(false);
    expected_event.set_net_error_code(0);
    expected_event.mutable_referrers()->Add(test::MakeUrlInfoReferrer());

    validator.ExpectSecurityInterstitialEvent(std::move(expected_event));
  } else {
    validator.ExpectSecurityInterstitialEventWithReferrers(
        "https://phishing.com/", "PHISHING", profile_->GetProfileUserName(),
        GetProfileIdentifier(), "EVENT_RESULT_BLOCKED", false, 0,
        test::MakeUrlInfoReferrer());
  }
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSecurityInterstitialShown(
      GURL("https://phishing.com/"), "PHISHING", 0, true, referrer_chain);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestInterstitialProceeded) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://phishing.com/");
    expected_event.set_reason(chrome::cros::reporting::proto::
                                  SafeBrowsingInterstitialEvent::PHISHING);
    expected_event.set_profile_user_name(profile_->GetProfileUserName());
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EVENT_RESULT_BYPASSED);
    expected_event.set_clicked_through(true);
    expected_event.set_net_error_code(0);
    expected_event.mutable_referrers()->Add(test::MakeUrlInfoReferrer());

    validator.ExpectSecurityInterstitialEvent(std::move(expected_event));
  } else {
    validator.ExpectSecurityInterstitialEventWithReferrers(
        "https://phishing.com/", "PHISHING", profile_->GetProfileUserName(),
        GetProfileIdentifier(), "EVENT_RESULT_BYPASSED", true, 0,
        test::MakeUrlInfoReferrer());
  }
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSecurityInterstitialProceeded(
      GURL("https://phishing.com/"), "PHISHING", 0, referrer_chain);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestPasswordReuseWarned) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordReuseEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingPasswordReuseEvent expected_event;
  if (use_proto_format()) {
    expected_event.set_url("https://phishing.com/");
    expected_event.set_user_name("user_name_1");
    expected_event.set_is_phishing_url(true);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EVENT_RESULT_WARNED);
    expected_event.set_profile_user_name(profile_->GetProfileUserName());
    expected_event.set_profile_identifier(GetProfileIdentifier());

    validator.ExpectPasswordReuseEvent(std::move(expected_event));
  } else {
    validator.ExpectPasswordReuseEvent(
        "https://phishing.com/", "user_name_1", true, "EVENT_RESULT_WARNED",
        profile_->GetProfileUserName(), GetProfileIdentifier());
  }

  reporting_event_router_->OnPasswordReuse(
      GURL("https://phishing.com/"), "user_name_1", /*is_phishing_url*/ true,
      /*warning_shown*/ true);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestPasswordReuseAllowed) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordReuseEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingPasswordReuseEvent expected_event;
  if (use_proto_format()) {
    expected_event.set_url("https://phishing.com/");
    expected_event.set_user_name("user_name_1");
    expected_event.set_is_phishing_url(true);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EVENT_RESULT_ALLOWED);
    expected_event.set_profile_user_name(profile_->GetProfileUserName());
    expected_event.set_profile_identifier(GetProfileIdentifier());

    validator.ExpectPasswordReuseEvent(std::move(expected_event));
  } else {
    validator.ExpectPasswordReuseEvent(
        "https://phishing.com/", "user_name_1", true, "EVENT_RESULT_ALLOWED",
        profile_->GetProfileUserName(), GetProfileIdentifier());
  }

  reporting_event_router_->OnPasswordReuse(
      GURL("https://phishing.com/"), "user_name_1", /*is_phishing_url*/ true,
      /*warning_shown*/ false);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestPasswordChanged) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordChangedEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingPasswordChangedEvent
      expected_event;

  if (use_proto_format()) {
    expected_event.set_user_name("user_name_1");
    expected_event.set_profile_user_name(profile_->GetProfileUserName());
    expected_event.set_profile_identifier(GetProfileIdentifier());

    validator.ExpectPasswordChangedEvent(std::move(expected_event));
  } else {
    validator.ExpectPassowrdChangedEvent(
        "user_name_1", profile_->GetProfileUserName(), GetProfileIdentifier());
  }

  reporting_event_router_->OnPasswordChanged("user_name_1");
  run_loop.Run();
}

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
TEST_P(ReportingEventRouterTest, TestOnUnscannedFileEvent_Allowed) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUnscannedFileEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::UnscannedFileEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("about:blank");
    expected_event.set_tab_url("tab:about:blank");
    expected_event.set_source("exampleSource");
    expected_event.set_destination("exampleDestination");
    expected_event.set_file_name("encrypted.zip");
    expected_event.set_download_digest_sha_256("sha256_of_data");
    expected_event.set_content_type("application/zip");
    expected_event.set_content_size(12345);

    expected_event.set_unscanned_reason(
        chrome::cros::reporting::proto::UnscannedFileEvent::
            FILE_PASSWORD_PROTECTED);
    expected_event.set_trigger(
        chrome::cros::reporting::proto::DataTransferEventTrigger::FILE_UPLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_ALLOWED);
    expected_event.set_clicked_through(false);
    expected_event.set_content_transfer_method(
        chrome::cros::reporting::proto::CONTENT_TRANSFER_METHOD_DRAG_AND_DROP);

    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_profile_user_name(profile_->GetProfileUserName());

    validator.ExpectUnscannedFileEvent(std::move(expected_event));
  } else {
    validator.ExpectUnscannedFileEvent(
        /*expected_url=*/"about:blank", /*expected_tab_url=*/"tab:about:blank",
        /*expected_source=*/"exampleSource",
        /*expected_destination=*/"exampleDestination",
        /*expected_filename=*/"encrypted.zip",
        /*expected_sha256=*/"sha256_of_data",
        /*expected_trigger=*/"FILE_UPLOAD",
        /*expected_reason=*/"FILE_PASSWORD_PROTECTED",
        /*expected_mimetypes=*/ZipMimeType(), /*expected_content_size=*/12345,
        /* expected_result=*/"EVENT_RESULT_ALLOWED",
        /*expected_profile_username=*/profile_->GetProfileUserName(),
        /*expected_profile_identifier=*/GetProfileIdentifier(),
        /*expected_content_transfer_method*/
        "CONTENT_TRANSFER_METHOD_DRAG_AND_DROP");
  }

  reporting_event_router_->OnUnscannedFileEvent(
      GURL("about:blank"), GURL("tab:about:blank"), "exampleSource",
      "exampleDestination", "encrypted.zip", "sha256_of_data",
      "application/zip", "FILE_UPLOAD", "FILE_PASSWORD_PROTECTED",
      "CONTENT_TRANSFER_METHOD_DRAG_AND_DROP", 12345, EventResult::ALLOWED);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestOnUnscannedFileEvent_Blocked) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUnscannedFileEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::UnscannedFileEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("about:blank");
    expected_event.set_tab_url("tab:about:blank");
    expected_event.set_source("exampleSource");
    expected_event.set_destination("exampleDestination");
    expected_event.set_file_name("encrypted.zip");
    expected_event.set_download_digest_sha_256("sha256_of_data");
    expected_event.set_content_type("application/zip");
    expected_event.set_content_size(12345);

    expected_event.set_unscanned_reason(
        chrome::cros::reporting::proto::UnscannedFileEvent::
            FILE_PASSWORD_PROTECTED);
    expected_event.set_trigger(chrome::cros::reporting::proto::
                                   DataTransferEventTrigger::FILE_DOWNLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BLOCKED);
    expected_event.set_clicked_through(false);
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_profile_user_name(profile_->GetProfileUserName());

    validator.ExpectUnscannedFileEvent(std::move(expected_event));
  } else {
    validator.ExpectUnscannedFileEvent(
        /*expected_url=*/"about:blank", /*expected_tab_url=*/"tab:about:blank",
        /*expected_source=*/"exampleSource",
        /*expected_destination=*/"exampleDestination",
        /*expected_filename=*/"encrypted.zip",
        /*expected_sha256=*/"sha256_of_data",
        /*expected_trigger=*/"FILE_DOWNLOAD",
        /*expected_reason=*/"FILE_PASSWORD_PROTECTED",
        /*expected_mimetypes=*/ZipMimeType(), /*expected_content_size=*/12345,
        /*expected_result=*/"EVENT_RESULT_BLOCKED",
        /*expected_profile_username=*/profile_->GetProfileUserName(),
        /*expected_profile_identifier=*/GetProfileIdentifier(),
        /*expected_content_transfer_method*/ std::nullopt);
  }

  reporting_event_router_->OnUnscannedFileEvent(
      GURL("about:blank"), GURL("tab:about:blank"), "exampleSource",
      "exampleDestination", "encrypted.zip", "sha256_of_data",
      "application/zip", "FILE_DOWNLOAD", "FILE_PASSWORD_PROTECTED", "", 12345,
      EventResult::BLOCKED);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestOnSensitiveDataEvent_Allowed) {
  EnableEnhancedFieldsForSecOps();

  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeySensitiveDataEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;

  ContentAnalysisResponse response;
  response.set_request_token("123");
  auto* result = response.add_results();
  result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  result->set_tag("dlp");

  if (use_proto_format()) {
    expected_event.set_url("about:blank");
    expected_event.set_tab_url("about:blank");
    expected_event.set_source("exampleSource");
    expected_event.set_destination("exampleDestination");
    expected_event.set_download_digest_sha_256("sha256_of_data");
    expected_event.set_file_name("encrypted.zip");
    expected_event.set_content_type("application/zip");
    expected_event.set_content_size(200);
    expected_event.set_scan_id("123");
    expected_event.set_trigger(
        chrome::cros::reporting::proto::DataTransferEventTrigger::FILE_UPLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_ALLOWED);
    expected_event.set_clicked_through(false);
    expected_event.set_content_transfer_method(
        chrome::cros::reporting::proto::CONTENT_TRANSFER_METHOD_DRAG_AND_DROP);
    expected_event.set_web_app_signed_in_account("gaia@gmail.com");
    expected_event.set_source_web_app_signed_in_account("test@gmail.com");

    *expected_event.add_referrers() = test::MakeUrlInfoReferrer();
    *expected_event.mutable_iframe_urls() = CreateFakeFrameUrlChainProto();

    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_profile_user_name(profile_->GetProfileUserName());

    validator.ExpectSensitiveDataEvent(std::move(expected_event));
  } else {
    validator.ExpectSensitiveDataEvent(
        /*url*/ "about:blank",
        /*tab_url*/ "about:blank",
        /*source*/ "exampleSource",
        /*destination*/ "exampleDestination",
        /*filename*/ "encrypted.zip",
        /*sha256*/ "sha256_of_data",
        /*trigger*/ "FILE_UPLOAD",
        /*dlp_verdict*/ *result,
        /*mimetype*/ ZipMimeType(),
        /*size*/ 200,
        /*result*/
        EventResultToString(EventResult::ALLOWED),
        /*username*/ profile_->GetProfileUserName(),
        /*profile_identifier*/ GetProfileIdentifier(),
        /*scan_id*/ "123",
        /*content_transfer_method*/ "CONTENT_TRANSFER_METHOD_DRAG_AND_DROP",
        /*user_justification*/ std::nullopt);
    validator.ExpectActiveUser("gaia@gmail.com");
    validator.ExpectSourceActiveUser("test@gmail.com");
    validator.ExpectFrameUrlChain(GetFakeFrameUrlChain());
  }

  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSensitiveDataEvent(
      GURL("about:blank"), GURL("about:blank"), "exampleSource",
      "exampleDestination", "encrypted.zip", "sha256_of_data",
      "application/zip", "FILE_UPLOAD", "123",
      "CONTENT_TRANSFER_METHOD_DRAG_AND_DROP", "test@gmail.com",
      "gaia@gmail.com", /*user_justification=*/std::nullopt, *result, 200,
      referrer_chain, CreateFakeFrameUrlChainProto(),
      EventResult::ALLOWED);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestOnSensitiveDataEvent_Blocked) {
  EnableEnhancedFieldsForSecOps();

  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeySensitiveDataEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;

  ContentAnalysisResponse response;
  response.set_request_token("123");
  auto* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* rule = result->add_triggered_rules();
  rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
  rule->set_rule_name("fake rule");
  rule->set_rule_id("12345");
  rule->set_url_category("test rule category");

  if (use_proto_format()) {
    expected_event.set_url("about:blank");
    expected_event.set_tab_url("about:blank");
    expected_event.set_source("exampleSource");
    expected_event.set_destination("exampleDestination");
    expected_event.set_download_digest_sha_256("sha256_of_data");
    expected_event.set_file_name("encrypted.zip");
    expected_event.set_content_type("application/zip");
    expected_event.set_content_size(200);
    expected_event.set_scan_id("123");
    expected_event.set_trigger(chrome::cros::reporting::proto::
                                   DataTransferEventTrigger::FILE_DOWNLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BLOCKED);
    expected_event.set_clicked_through(false);
    expected_event.set_web_app_signed_in_account("gaia@gmail.com");
    expected_event.set_source_web_app_signed_in_account("test@gmail.com");

    TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_id(12345);
    triggered_rule.set_rule_name("fake rule");
    triggered_rule.set_url_category("test rule category");
    triggered_rule.set_action(
        chrome::cros::reporting::proto::TriggeredRuleInfo::BLOCK);

    *expected_event.add_triggered_rule_info() = triggered_rule;
    *expected_event.add_referrers() = test::MakeUrlInfoReferrer();
    *expected_event.mutable_iframe_urls() = CreateFakeFrameUrlChainProto();

    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_profile_user_name(profile_->GetProfileUserName());

    validator.ExpectSensitiveDataEvent(std::move(expected_event));
  } else {
    validator.ExpectSensitiveDataEvent(
        /*url*/ "about:blank",
        /*tab_url*/ "about:blank",
        /*source*/ "exampleSource",
        /*destination*/ "exampleDestination",
        /*filename*/ "encrypted.zip",
        /*sha256*/ "sha256_of_data",
        /*trigger*/ "FILE_DOWNLOAD",
        /*dlp_verdict*/ *result,
        /*mimetype*/ ZipMimeType(),
        /*size*/ 200,
        /*result*/
        EventResultToString(EventResult::BLOCKED),
        /*username*/ profile_->GetProfileUserName(),
        /*profile_identifier*/ GetProfileIdentifier(),
        /*scan_id*/ "123",
        /*content_transfer_method*/ std::nullopt,
        /*user_justification*/ std::nullopt);
    validator.ExpectActiveUser("gaia@gmail.com");
    validator.ExpectSourceActiveUser("test@gmail.com");
    validator.ExpectFrameUrlChain(GetFakeFrameUrlChain());
  }

  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSensitiveDataEvent(
      GURL("about:blank"), GURL("about:blank"), "exampleSource",
      "exampleDestination", "encrypted.zip", "sha256_of_data",
      "application/zip", "FILE_DOWNLOAD", "123", "", "test@gmail.com",
      "gaia@gmail.com", /*user_justification=*/std::nullopt, *result, 200,
      referrer_chain, CreateFakeFrameUrlChainProto(),
      EventResult::BLOCKED);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestOnDangerousDownloadEvent_Warned) {
  EnableEnhancedFieldsForSecOps();

  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyDangerousDownloadEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent
      expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://example.com/download.exe");
    expected_event.set_tab_url("https://example.com/");
    expected_event.set_source("exampleSource");
    expected_event.set_destination("exampleDestination");
    expected_event.set_download_digest_sha256("sha256_of_data");
    expected_event.set_threat_type(
        chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent::
            POTENTIALLY_UNWANTED);
    expected_event.set_file_name("encrypted.zip");
    expected_event.set_content_type("application/zip");
    expected_event.set_content_size(12345);
    expected_event.set_scan_id("123");
    expected_event.set_trigger(chrome::cros::reporting::proto::
                                   DataTransferEventTrigger::FILE_DOWNLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);
    expected_event.set_clicked_through(false);

    *expected_event.add_referrers() = test::MakeUrlInfoReferrer();
    *expected_event.mutable_iframe_urls() = CreateFakeFrameUrlChainProto();

    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_profile_user_name(profile_->GetProfileUserName());

    validator.ExpectDangerousDownloadEvent(std::move(expected_event));
  } else {
    validator.ExpectDangerousDeepScanningResult(
        /*url*/ "https://example.com/download.exe",
        /*tab_url*/ "https://example.com/",
        /*source*/ "exampleSource",
        /*destination*/ "exampleDestination",
        /*filename*/ "encrypted.zip",
        /*sha256*/ "sha256_of_data",
        /*threat_type*/ "POTENTIALLY_UNWANTED",
        /*trigger*/ "FILE_DOWNLOAD",
        /*mimetypes*/ ZipMimeType(),
        /*size*/ 12345,
        /*result*/ "EVENT_RESULT_WARNED",
        /*username*/ profile_->GetProfileUserName(),
        /*profile_identifier*/ GetProfileIdentifier(),
        /*scan_id*/ "123");
    validator.ExpectFrameUrlChain(GetFakeFrameUrlChain());
  }

  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnDangerousDownloadEvent(
      GURL("https://example.com/download.exe"), GURL("https://example.com/"),
      "exampleSource", "exampleDestination", "encrypted.zip", "sha256_of_data",
      "POTENTIALLY_UNWANTED", "application/zip", "FILE_DOWNLOAD", "123",
      /*content_transfer_method=*/"", 12345, std::move(referrer_chain),
      CreateFakeFrameUrlChainProto(), EventResult::WARNED);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestOnDangerousDownloadEvent_Blocked) {
  EnableEnhancedFieldsForSecOps();

  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyDangerousDownloadEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent
      expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://example.com/download.exe");
    expected_event.set_tab_url("https://example.com/");
    expected_event.set_source("exampleSource");
    expected_event.set_destination("exampleDestination");
    expected_event.set_download_digest_sha256("sha256_of_data");
    expected_event.set_threat_type(
        chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent::
            DANGEROUS);
    expected_event.set_file_name("encrypted.zip");
    expected_event.set_content_type("application/zip");
    expected_event.set_content_size(12345);
    expected_event.set_scan_id("123");
    expected_event.set_trigger(chrome::cros::reporting::proto::
                                   DataTransferEventTrigger::FILE_DOWNLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BLOCKED);
    expected_event.set_clicked_through(false);

    *expected_event.add_referrers() = test::MakeUrlInfoReferrer();
    *expected_event.mutable_iframe_urls() = CreateFakeFrameUrlChainProto();

    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_profile_user_name(profile_->GetProfileUserName());

    validator.ExpectDangerousDownloadEvent(std::move(expected_event));
  } else {
    validator.ExpectDangerousDeepScanningResult(
        /*url*/ "https://example.com/download.exe",
        /*tab_url*/ "https://example.com/",
        /*source*/ "exampleSource",
        /*destination*/ "exampleDestination",
        /*filename*/ "encrypted.zip",
        /*sha256*/ "sha256_of_data",
        /*threat_type*/ "DANGEROUS",
        /*trigger*/ "FILE_DOWNLOAD",
        /*mimetypes*/ ZipMimeType(),
        /*size*/ 12345,
        /*result*/ "EVENT_RESULT_BLOCKED",
        /*username*/ profile_->GetProfileUserName(),
        /*profile_identifier*/ GetProfileIdentifier(),
        /*scan_id*/ "123");
    validator.ExpectFrameUrlChain(GetFakeFrameUrlChain());
  }

  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnDangerousDownloadEvent(
      GURL("https://example.com/download.exe"), GURL("https://example.com/"),
      "exampleSource", "exampleDestination", "encrypted.zip", "sha256_of_data",
      "DANGEROUS", "application/zip", "FILE_DOWNLOAD", "123",
      /*content_transfer_method=*/"", 12345, std::move(referrer_chain),
      CreateFakeFrameUrlChainProto(), EventResult::BLOCKED);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest, TestOnDangerousDownloadEvent_Bypassed) {
  EnableEnhancedFieldsForSecOps();

  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyDangerousDownloadEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent
      expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://example.com/download.exe");
    expected_event.set_tab_url("https://example.com/");
    expected_event.set_source("");
    expected_event.set_destination("");
    expected_event.set_download_digest_sha256("sha256_of_data");
    expected_event.set_threat_type(
        chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent::
            DANGEROUS);
    expected_event.set_file_name("encrypted.zip");
    expected_event.set_content_type("application/zip");
    expected_event.set_content_size(12345);
    expected_event.set_scan_id("123");
    expected_event.set_trigger(chrome::cros::reporting::proto::
                                   DataTransferEventTrigger::FILE_DOWNLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BYPASSED);
    expected_event.set_clicked_through(true);

    *expected_event.add_referrers() = test::MakeUrlInfoReferrer();
    *expected_event.mutable_iframe_urls() = CreateFakeFrameUrlChainProto();

    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_profile_user_name(profile_->GetProfileUserName());

    validator.ExpectDangerousDownloadEvent(std::move(expected_event));
  } else {
    validator.ExpectDangerousDeepScanningResult(
        /*url*/ "https://example.com/download.exe",
        /*tab_url*/ "https://example.com/",
        /*source*/ "",
        /*destination*/ "",
        /*filename*/ "encrypted.zip",
        /*sha256*/ "sha256_of_data",
        /*threat_type*/ "DANGEROUS",
        /*trigger*/ "FILE_DOWNLOAD",
        /*mimetypes*/ ZipMimeType(),
        /*size*/ 12345,
        /*result*/ "EVENT_RESULT_BYPASSED",
        /*username*/ profile_->GetProfileUserName(),
        /*profile_identifier*/ GetProfileIdentifier(),
        /*scan_id*/ "123");
    validator.ExpectFrameUrlChain(GetFakeFrameUrlChain());
  }

  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnDangerousDownloadEvent(
      GURL("https://example.com/download.exe"), GURL("https://example.com/"),
      "encrypted.zip", "sha256_of_data",
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT, "application/zip",
      "FILE_DOWNLOAD", "123", 12345, std::move(referrer_chain),
      CreateFakeFrameUrlChainProto(), EventResult::BYPASSED);
  run_loop.Run();
}

TEST_P(ReportingEventRouterTest,
       TestOnDangerousDownloadEvent_WarnedFromSafeBrowsing) {
  EnableEnhancedFieldsForSecOps();

  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyDangerousDownloadEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent
      expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://example.com/download.exe");
    expected_event.set_tab_url("https://example.com/");
    expected_event.set_source("");
    expected_event.set_destination("");
    expected_event.set_download_digest_sha256("sha256_of_data");
    expected_event.set_threat_type(
        chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent::
            DANGEROUS);
    expected_event.set_file_name("encrypted.zip");
    expected_event.set_content_type("application/zip");
    expected_event.set_content_size(12345);
    expected_event.set_trigger(chrome::cros::reporting::proto::
                                   DataTransferEventTrigger::FILE_DOWNLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);
    expected_event.set_clicked_through(false);

    *expected_event.add_referrers() = test::MakeUrlInfoReferrer();
    *expected_event.mutable_iframe_urls() = CreateFakeFrameUrlChainProto();

    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_profile_user_name(profile_->GetProfileUserName());

    validator.ExpectDangerousDownloadEvent(std::move(expected_event));
  } else {
    validator.ExpectDangerousDeepScanningResult(
        /*url*/ "https://example.com/download.exe",
        /*tab_url*/ "https://example.com/",
        /*source*/ "",
        /*destination*/ "",
        /*filename*/ "encrypted.zip",
        /*sha256*/ "sha256_of_data",
        /*threat_type*/ "DANGEROUS",
        /*trigger*/ "FILE_DOWNLOAD",
        /*mimetypes*/ ZipMimeType(),
        /*size*/ 12345,
        /*result*/ "EVENT_RESULT_WARNED",
        /*username*/ profile_->GetProfileUserName(),
        /*profile_identifier*/ GetProfileIdentifier(),
        /*scan_id*/ std::nullopt);
    validator.ExpectFrameUrlChain(GetFakeFrameUrlChain());
  }

  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnDangerousDownloadEvent(
      GURL("https://example.com/download.exe"), GURL("https://example.com/"),
      "encrypted.zip", "sha256_of_data",
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT, "application/zip",
      "FILE_DOWNLOAD", "", 12345, std::move(referrer_chain),
      CreateFakeFrameUrlChainProto(), EventResult::WARNED);
  run_loop.Run();
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
TEST_P(ReportingEventRouterTest, TestOnDataControlsSensitiveDataEvent) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeySensitiveDataEvent},
      /*enabled_opt_in_events=*/{});

  data_controls::Verdict::TriggeredRules triggered_rules = {
      {{0, true}, {"1", "rule_1_name"}}};
  test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://example.com/");
    expected_event.set_tab_url("https://example.com/");
    expected_event.set_source("exampleSource");
    expected_event.set_destination("exampleDestination");
    expected_event.set_content_type("text/html");
    expected_event.set_content_size(1234);
    expected_event.set_trigger(
        chrome::cros::reporting::proto::DataTransferEventTrigger::
            WEB_CONTENT_UPLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_ALLOWED);
    expected_event.set_web_app_signed_in_account("content_area_user@gmail.com");
    expected_event.set_source_web_app_signed_in_account(
        "active_user@gmail.com");

    TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_id(1);
    triggered_rule.set_rule_name("rule_1_name");

    *expected_event.add_triggered_rule_info() = triggered_rule;
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_profile_user_name(profile_->GetProfileUserName());

    validator.ExpectSensitiveDataEvent(std::move(expected_event));
  } else {
    validator.ExpectDataControlsSensitiveDataEvent(
        /*expected_url*/
        "https://example.com/",
        /*expected_tab_url*/ "https://example.com/",
        /*expected_source*/ "exampleSource",
        /*expected_destination*/ "exampleDestination",
        /*expected_mimetypes=*/
        []() {
          static std::set<std::string> set = {"text/html"};
          return &set;
        }(),
        /*expected_trigger=*/"WEB_CONTENT_UPLOAD",
        /*triggered_rules=*/triggered_rules,
        /*expected_result*/ "EVENT_RESULT_ALLOWED",
        /*expected_profile_username*/ profile_->GetProfileUserName(),
        /*expected_profile_identifier*/ GetProfileIdentifier(),
        /*expected_content_size=*/1234);
    validator.ExpectActiveUser("content_area_user@gmail.com");
    validator.ExpectSourceActiveUser("active_user@gmail.com");
  }

  reporting_event_router_->OnDataControlsSensitiveDataEvent(
      GURL("https://example.com/"), GURL("https://example.com/"),
      "exampleSource", "exampleDestination", "text/html",
      enterprise_connectors::kWebContentUploadDataTransferEventTrigger,
      "active_user@gmail.com", "content_area_user@gmail.com", triggered_rules,
      enterprise_connectors::EventResult::ALLOWED, 1234);
  run_loop.Run();
}
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

INSTANTIATE_TEST_SUITE_P(, ReportingEventRouterTest, ::testing::Bool());

}  // namespace enterprise_connectors
