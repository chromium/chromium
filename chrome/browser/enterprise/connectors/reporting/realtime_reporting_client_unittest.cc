// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
#include "components/enterprise/browser/enterprise_switches.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "base/notreached.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "components/device_signals/core/browser/signals_types.h"
#endif

using testing::_;

namespace enterprise_connectors {

namespace {

using Event = ::chrome::cros::reporting::proto::Event;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

using SecurityAgent = ::chrome::cros::reporting::proto::SecurityAgent;

Event GetTestEvent(Event::EventCase event_case) {
  Event event;
  switch (event_case) {
    case Event::kPasswordReuseEvent:
      event.mutable_password_reuse_event();
      break;
    case Event::kPasswordChangedEvent:
      event.mutable_password_changed_event();
      break;
    case Event::kDangerousDownloadEvent:
      event.mutable_dangerous_download_event();
      break;
    case Event::kInterstitialEvent:
      event.mutable_interstitial_event();
      break;
    case Event::kSensitiveDataEvent:
      event.mutable_sensitive_data_event();
      break;
    case Event::kUnscannedFileEvent:
      event.mutable_unscanned_file_event();
      break;
    case Event::kLoginEvent:
      event.mutable_login_event();
      break;
    case Event::kPasswordBreachEvent:
      event.mutable_password_breach_event();
      break;
    case Event::kBrowserExtensionInstallEvent:
      event.mutable_browser_extension_install_event();
      break;
    case Event::kBrowserCrashEvent:
      event.mutable_browser_crash_event();
      break;
    case Event::kUrlFilteringInterstitialEvent:
      event.mutable_url_filtering_interstitial_event();
      break;
    case Event::kExtensionTelemetryEvent:
      event.mutable_extension_telemetry_event();
      break;
    default:
      NOTREACHED();
  }
  return event;
}

google::protobuf::RepeatedPtrField<SecurityAgent> GetSecurityAgents(
    Event event) {
  switch (event.event_case()) {
    case Event::kPasswordReuseEvent:
      return event.password_reuse_event().security_agents();
    case Event::kPasswordChangedEvent:
      return event.password_changed_event().security_agents();
    case Event::kDangerousDownloadEvent:
      return event.dangerous_download_event().security_agents();
    case Event::kInterstitialEvent:
      return event.interstitial_event().security_agents();
    case Event::kSensitiveDataEvent:
      return event.sensitive_data_event().security_agents();
    case Event::kUnscannedFileEvent:
      return event.unscanned_file_event().security_agents();
    case Event::kLoginEvent:
      return event.login_event().security_agents();
    case Event::kPasswordBreachEvent:
      return event.password_breach_event().security_agents();
    case Event::kBrowserExtensionInstallEvent:
      return event.browser_extension_install_event().security_agents();
    case Event::kBrowserCrashEvent:
      return event.browser_crash_event().security_agents();
    case Event::kUrlFilteringInterstitialEvent:
      return event.url_filtering_interstitial_event().security_agents();
    case Event::kExtensionTelemetryEvent:
      return event.extension_telemetry_event().security_agents();
    default:
      NOTREACHED();
  }
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

std::unique_ptr<KeyedService> BuildRealtimeReportingClient(
    content::BrowserContext* context) {
  return std::unique_ptr<KeyedService>(new RealtimeReportingClient(context));
}

class RealtimeReportingClientTestBase : public testing::Test {
 public:
  RealtimeReportingClientTestBase()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
  }

  RealtimeReportingClientTestBase(const RealtimeReportingClientTestBase&) =
      delete;
  RealtimeReportingClientTestBase& operator=(
      const RealtimeReportingClientTestBase&) = delete;

  ~RealtimeReportingClientTestBase() override = default;

  void SetUp() override {
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));

    if (!client_) {
      // Set a mock cloud policy client in the router.
      client_ = std::make_unique<policy::MockCloudPolicyClient>();
      client_->SetDMToken("fake-token");
    }

    RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&BuildRealtimeReportingClient));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
};
}  // namespace

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class RealtimeReportingClientOidcTest : public RealtimeReportingClientTestBase {
 public:
  RealtimeReportingClientOidcTest() {
    scoped_feature_list_.InitAndEnableFeature(
        profile_management::features::kOidcAuthProfileManagement);
  }

  void SetUp() override {
    RealtimeReportingClientTestBase::SetUp();
    profile_->GetPrefs()->SetString(enterprise_signin::prefs::kProfileUserEmail,
                                    "oidc@user.email");
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

class RealtimeReportingClientUmaTest
    : public RealtimeReportingClientTestBase,
      public testing::WithParamInterface<bool> {
 public:
  bool is_profile_reporting() { return GetParam(); }

  void SetUp() override {
    RealtimeReportingClientTestBase::SetUp();
    reporting_client_ =
        enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
            profile_);
  }

 protected:
  base::HistogramTester histogram_;
  raw_ptr<RealtimeReportingClient> reporting_client_ = nullptr;
  policy::CloudPolicyClient::ResultCallback upload_callback_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(RealtimeReportingClientUmaTest, TestDeprecatedUmaEventUploadSucceeds) {
// Profile reporting is not supported on Ash.
#if BUILDFLAG(IS_CHROMEOS)
  if (is_profile_reporting()) {
    return;
  }
#endif

  is_profile_reporting()
      ? reporting_client_->SetProfileCloudPolicyClientForTesting(client_.get())
      : reporting_client_->SetBrowserCloudPolicyClientForTesting(client_.get());

  ReportingSettings settings;
  settings.per_profile = is_profile_reporting();
  base::Value::Dict event;

  base::RunLoop run_loop;
  EXPECT_CALL(*client_.get(), UploadSecurityEventReport(_, _, _))
      .WillOnce([&](bool include_device_info, base::Value::Dict&& report,
                    policy::CloudPolicyClient::ResultCallback callback) {
        upload_callback_ = std::move(callback);
        run_loop.Quit();
      });
  reporting_client_->ReportRealtimeEvent(kExtensionInstallEvent,
                                         std::move(settings), std::move(event));
  run_loop.Run();

  ASSERT_TRUE(upload_callback_);
  std::move(upload_callback_)
      .Run(policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));

  histogram_.ExpectUniqueSample(
      "Enterprise.ReportingEventUploadSuccess",
      EnterpriseReportingEventType::kExtensionInstallEvent, 1);
  histogram_.ExpectTotalCount("Enterprise.ReportingEventUploadFailure", 0);
}

TEST_P(RealtimeReportingClientUmaTest, TestUmaEventUploadSucceeds) {
// Profile reporting is not supported on Ash.
#if BUILDFLAG(IS_CHROMEOS)
  if (is_profile_reporting()) {
    return;
  }
#endif

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      policy::kUploadRealtimeReportingEventsUsingProto);

  is_profile_reporting()
      ? reporting_client_->SetProfileCloudPolicyClientForTesting(client_.get())
      : reporting_client_->SetBrowserCloudPolicyClientForTesting(client_.get());

  ReportingSettings settings;
  settings.per_profile = is_profile_reporting();
  ::chrome::cros::reporting::proto::Event extension_install_event;
  extension_install_event.mutable_browser_extension_install_event()->set_id(
      "extension_id");

  EXPECT_EQ(EnterpriseReportingEventType::kExtensionInstallEvent,
            enterprise_connectors::GetUmaEnumFromEventCase(
                extension_install_event.event_case()));
  EXPECT_EQ(::chrome::cros::reporting::proto::Event::EventCase::
                kBrowserExtensionInstallEvent,
            extension_install_event.event_case());

  base::RunLoop run_loop;
  EXPECT_CALL(*client_.get(), UploadSecurityEvent(_, _, _))
      .WillOnce(
          [&](bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest&& request,
              policy::CloudPolicyClient::ResultCallback callback) {
            upload_callback_ = std::move(callback);
            run_loop.Quit();
          });
  reporting_client_->ReportEvent(std::move(extension_install_event),
                                 std::move(settings));
  run_loop.Run();

  ASSERT_TRUE(upload_callback_);
  std::move(upload_callback_)
      .Run(policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));

  histogram_.ExpectUniqueSample(
      "Enterprise.ReportingEventUploadSuccess",
      EnterpriseReportingEventType::kExtensionInstallEvent, 1);
  histogram_.ExpectTotalCount("Enterprise.ReportingEventUploadFailure", 0);
}

TEST_P(RealtimeReportingClientUmaTest, TestDeprecatedUmaEventUploadFails) {
// Profile reporting is not supported on Ash.
#if BUILDFLAG(IS_CHROMEOS)
  if (is_profile_reporting()) {
    return;
  }
#endif

  is_profile_reporting()
      ? reporting_client_->SetProfileCloudPolicyClientForTesting(client_.get())
      : reporting_client_->SetBrowserCloudPolicyClientForTesting(client_.get());

  ReportingSettings settings;
  settings.per_profile = is_profile_reporting();
  base::Value::Dict event;

  base::RunLoop run_loop;
  EXPECT_CALL(*client_.get(), UploadSecurityEventReport(_, _, _))
      .WillOnce([&](bool include_device_info, base::Value::Dict&& report,
                    policy::CloudPolicyClient::ResultCallback callback) {
        upload_callback_ = std::move(callback);
        run_loop.Quit();
      });
  reporting_client_->ReportRealtimeEvent(kExtensionInstallEvent,
                                         std::move(settings), std::move(event));
  run_loop.Run();

  ASSERT_TRUE(upload_callback_);
  std::move(upload_callback_)
      .Run(policy::CloudPolicyClient::Result(policy::DM_STATUS_REQUEST_FAILED));

  histogram_.ExpectUniqueSample(
      "Enterprise.ReportingEventUploadFailure",
      EnterpriseReportingEventType::kExtensionInstallEvent, 1);
  histogram_.ExpectTotalCount("Enterprise.ReportingEventUploadSuccess", 0);
}

TEST_P(RealtimeReportingClientUmaTest, TestUmaEventUploadFails) {
// Profile reporting is not supported on Ash.
#if BUILDFLAG(IS_CHROMEOS)
  if (is_profile_reporting()) {
    return;
  }
#endif

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      policy::kUploadRealtimeReportingEventsUsingProto);

  is_profile_reporting()
      ? reporting_client_->SetProfileCloudPolicyClientForTesting(client_.get())
      : reporting_client_->SetBrowserCloudPolicyClientForTesting(client_.get());

  ReportingSettings settings;
  settings.per_profile = is_profile_reporting();
  ::chrome::cros::reporting::proto::Event extension_install_event;
  extension_install_event.mutable_browser_extension_install_event()->set_id(
      "extension_id");

  EXPECT_EQ(EnterpriseReportingEventType::kExtensionInstallEvent,
            enterprise_connectors::GetUmaEnumFromEventCase(
                extension_install_event.event_case()));
  EXPECT_EQ(::chrome::cros::reporting::proto::Event::EventCase::
                kBrowserExtensionInstallEvent,
            extension_install_event.event_case());

  base::RunLoop run_loop;
  EXPECT_CALL(*client_.get(), UploadSecurityEvent(_, _, _))
      .WillOnce(
          [&](bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest&& request,
              policy::CloudPolicyClient::ResultCallback callback) {
            upload_callback_ = std::move(callback);
            run_loop.Quit();
          });
  reporting_client_->ReportEvent(std::move(extension_install_event),
                                 std::move(settings));
  run_loop.Run();

  ASSERT_TRUE(upload_callback_);
  std::move(upload_callback_)
      .Run(policy::CloudPolicyClient::Result(policy::DM_STATUS_REQUEST_FAILED));

  histogram_.ExpectUniqueSample(
      "Enterprise.ReportingEventUploadFailure",
      EnterpriseReportingEventType::kExtensionInstallEvent, 1);
  histogram_.ExpectTotalCount("Enterprise.ReportingEventUploadSuccess", 0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         RealtimeReportingClientUmaTest,
                         /* is_profile_reporting */ testing::Bool());

TEST_F(RealtimeReportingClientTestBase,
       TestEventNameToUmaEnumMapIncludesAllEvents) {
  std::set<std::string> all_reporting_events;
  all_reporting_events.insert(kAllReportingEnabledEvents.begin(),
                              kAllReportingEnabledEvents.end());
  all_reporting_events.insert(kAllReportingOptInEvents.begin(),
                              kAllReportingOptInEvents.end());

  EXPECT_EQ(all_reporting_events.size(), kEventNameToUmaEnumMap.size());
  for (std::string eventName : all_reporting_events) {
    EXPECT_TRUE(kEventNameToUmaEnumMap.contains(eventName));
  }
}

TEST_F(RealtimeReportingClientTestBase,
       TestUnknownEventNameMapsTokUnknownEvent) {
  EXPECT_EQ(GetUmaEnumFromEventName("non-existent-event-name"),
            EnterpriseReportingEventType::kUnknownEvent);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

TEST_F(RealtimeReportingClientTestBase, TestCrowdstrikeSignalsPopulated) {
  base::Value::Dict event;
  device_signals::CrowdStrikeSignals signals;
  signals.agent_id = "agent-123";
  signals.customer_id = "customer-123";
  device_signals::AgentSignalsResponse agent_signals;
  agent_signals.crowdstrike_signals = signals;
  device_signals::SignalsAggregationResponse response;
  response.agent_signals_response = agent_signals;
  AddCrowdstrikeSignalsToEvent(event, response);
  const base::Value::List& agentList = event.Find("securityAgents")->GetList();
  ASSERT_EQ(agentList.size(), 1u);
  const base::Value::Dict& signalValues =
      agentList[0].GetDict().Find("crowdstrike")->GetDict();
  EXPECT_EQ(signalValues.Find("agent_id")->GetString(), "agent-123");
  EXPECT_EQ(signalValues.Find("customer_id")->GetString(), "customer-123");
}

TEST_F(RealtimeReportingClientTestBase,
       TestCrowdstrikeSignalsNotPopulatedForEmptyResponse) {
  base::Value::Dict event;
  device_signals::SignalsAggregationResponse response;
  response.agent_signals_response = std::nullopt;
  AddCrowdstrikeSignalsToEvent(event, response);
  EXPECT_EQ(event.Find("securityAgents"), nullptr);
}

TEST_F(RealtimeReportingClientOidcTest, Username) {
  RealtimeReportingClient client(profile_);
  ASSERT_EQ(client.GetProfileUserName(), "oidc@user.email");
}

// TODO(b/342232001): Add more tests for the `RealtimeReportingClientOidcTest`
// fixture to cover key use cases.

class ProtoBasedCrowdStrikeSignalTest
    : public RealtimeReportingClientTestBase,
      public testing::WithParamInterface<Event::EventCase> {
 public:
  ProtoBasedCrowdStrikeSignalTest() = default;
};

TEST_P(ProtoBasedCrowdStrikeSignalTest, TestCrowdstrikeSignalsPopulated) {
  device_signals::CrowdStrikeSignals signals;
  signals.agent_id = "agent-123";
  signals.customer_id = "customer-123";
  device_signals::AgentSignalsResponse agent_signals;
  agent_signals.crowdstrike_signals = signals;
  device_signals::SignalsAggregationResponse response;
  response.agent_signals_response = agent_signals;

  auto event = GetTestEvent(GetParam());
  AddCrowdstrikeSignalsToEvent(event, response);
  auto security_agents = GetSecurityAgents(event);

  ASSERT_EQ(security_agents.size(), 1);
  auto security_agent = security_agents[0];
  ASSERT_TRUE(security_agent.has_crowdstrike());
  EXPECT_EQ(security_agent.crowdstrike().agent_id(), "agent-123");
  EXPECT_EQ(security_agent.crowdstrike().customer_id(), "customer-123");
}

TEST_P(ProtoBasedCrowdStrikeSignalTest,
       TestCrowdstrikeSignalsNotPopulatedForEmptyResponse) {
  device_signals::SignalsAggregationResponse response;
  auto event = GetTestEvent(GetParam());
  AddCrowdstrikeSignalsToEvent(event, response);
  auto security_agents = GetSecurityAgents(event);

  ASSERT_EQ(security_agents.size(), 0);
}

INSTANTIATE_TEST_SUITE_P(,
                         ProtoBasedCrowdStrikeSignalTest,
                         testing::Values(Event::kPasswordReuseEvent,
                                         Event::kPasswordChangedEvent,
                                         Event::kDangerousDownloadEvent,
                                         Event::kInterstitialEvent,
                                         Event::kSensitiveDataEvent,
                                         Event::kUnscannedFileEvent,
                                         Event::kLoginEvent,
                                         Event::kPasswordBreachEvent,
                                         Event::kBrowserExtensionInstallEvent,
                                         Event::kBrowserCrashEvent,
                                         Event::kUrlFilteringInterstitialEvent,
                                         Event::kExtensionTelemetryEvent));

#endif

class RealtimeReportingClientUrlTruncationTest
    : public RealtimeReportingClientTestBase,
      public testing::WithParamInterface<Event::EventCase> {
 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      policy::kUploadRealtimeReportingEventsUsingProto};
};

TEST_P(RealtimeReportingClientUrlTruncationTest, TestUrlTruncation) {
  RealtimeReportingClient* reporting_client =
      enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
          profile_);
  reporting_client->SetBrowserCloudPolicyClientForTesting(client_.get());

  ReportingSettings settings;
  settings.per_profile = false;

  Event event_variant;
  Event::EventCase event_case = GetParam();

  std::string long_url(4242, 'a');
  std::string truncated_url(2048, 'a');

  switch (event_case) {
    case Event::kDangerousDownloadEvent: {
      auto* event = event_variant.mutable_dangerous_download_event();
      event->set_url(long_url);
      event->set_tab_url(long_url);
      event->mutable_url_info()->set_url(long_url);
      event->mutable_tab_url_info()->set_url(long_url);
      event->add_referral_urls(long_url);
      event->add_referrers()->set_url(long_url);
      event->add_iframe_urls(long_url);
      break;
    }
    case Event::kUrlFilteringInterstitialEvent: {
      auto* event = event_variant.mutable_url_filtering_interstitial_event();
      event->set_url(long_url);
      event->mutable_url_info()->set_url(long_url);
      event->add_referrer_urls(long_url);
      event->add_referrers()->set_url(long_url);
      break;
    }
    case Event::kPasswordBreachEvent: {
      auto* event = event_variant.mutable_password_breach_event();
      event->add_identities()->set_url(long_url);
      break;
    }
    case Event::kLoginEvent: {
      auto* event = event_variant.mutable_login_event();
      event->set_url(long_url);
      event->set_federated_origin(long_url);
      break;
    }
    case Event::kUnscannedFileEvent: {
      auto* event = event_variant.mutable_unscanned_file_event();
      event->set_url(long_url);
      event->set_tab_url(long_url);
      event->add_referral_urls(long_url);
      break;
    }
    case Event::kSensitiveDataEvent: {
      auto* event = event_variant.mutable_sensitive_data_event();
      event->set_url(long_url);
      event->set_tab_url(long_url);
      event->mutable_url_info()->set_url(long_url);
      event->add_referral_urls(long_url);
      event->add_referrers()->set_url(long_url);
      event->add_iframe_urls(long_url);
      break;
    }
    case Event::kInterstitialEvent: {
      auto* event = event_variant.mutable_interstitial_event();
      event->set_url(long_url);
      event->add_referral_urls(long_url);
      event->mutable_url_info()->set_url(long_url);
      event->add_referrers()->set_url(long_url);
      break;
    }
    case Event::kPasswordReuseEvent: {
      auto* event = event_variant.mutable_password_reuse_event();
      event->set_url(long_url);
      event->add_referral_urls(long_url);
      break;
    }
    default:
      GTEST_SKIP() << "Event type not yet supported for this test.";
  }

  base::RunLoop run_loop;
  EXPECT_CALL(*client_.get(), UploadSecurityEvent(_, _, _))
      .WillOnce(
          [event_case, truncated_url, &run_loop](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest&& request,
              policy::CloudPolicyClient::ResultCallback callback) {
            ASSERT_EQ(request.events_size(), 1);
            const auto& reported_event = request.events(0);
            ASSERT_EQ(reported_event.event_case(), event_case);

            switch (event_case) {
              case Event::kDangerousDownloadEvent: {
                const auto& event = reported_event.dangerous_download_event();
                EXPECT_EQ(event.url(), truncated_url);
                EXPECT_EQ(event.tab_url(), truncated_url);
                EXPECT_EQ(event.url_info().url(), truncated_url);
                EXPECT_EQ(event.tab_url_info().url(), truncated_url);
                ASSERT_EQ(event.referral_urls_size(), 1);
                EXPECT_EQ(event.referral_urls(0), truncated_url);
                ASSERT_EQ(event.referrers_size(), 1);
                EXPECT_EQ(event.referrers(0).url(), truncated_url);
                ASSERT_EQ(event.iframe_urls_size(), 1);
                EXPECT_EQ(event.iframe_urls(0), truncated_url);
                break;
              }
              case Event::kUrlFilteringInterstitialEvent: {
                const auto& event =
                    reported_event.url_filtering_interstitial_event();
                EXPECT_EQ(event.url(), truncated_url);
                EXPECT_EQ(event.url_info().url(), truncated_url);
                ASSERT_EQ(event.referrer_urls_size(), 1);
                EXPECT_EQ(event.referrer_urls(0), truncated_url);
                ASSERT_EQ(event.referrers_size(), 1);
                EXPECT_EQ(event.referrers(0).url(), truncated_url);
                break;
              }
              case Event::kPasswordBreachEvent: {
                const auto& event = reported_event.password_breach_event();
                ASSERT_EQ(event.identities_size(), 1);
                EXPECT_EQ(event.identities(0).url(), truncated_url);
                break;
              }
              case Event::kLoginEvent: {
                const auto& event = reported_event.login_event();
                EXPECT_EQ(event.url(), truncated_url);
                EXPECT_EQ(event.federated_origin(), truncated_url);
                break;
              }
              case Event::kUnscannedFileEvent: {
                const auto& event = reported_event.unscanned_file_event();
                EXPECT_EQ(event.url(), truncated_url);
                EXPECT_EQ(event.tab_url(), truncated_url);
                ASSERT_EQ(event.referral_urls_size(), 1);
                EXPECT_EQ(event.referral_urls(0), truncated_url);
                break;
              }
              case Event::kSensitiveDataEvent: {
                const auto& event = reported_event.sensitive_data_event();
                EXPECT_EQ(event.url(), truncated_url);
                EXPECT_EQ(event.tab_url(), truncated_url);
                EXPECT_EQ(event.url_info().url(), truncated_url);
                ASSERT_EQ(event.referral_urls_size(), 1);
                EXPECT_EQ(event.referral_urls(0), truncated_url);
                ASSERT_EQ(event.referrers_size(), 1);
                EXPECT_EQ(event.referrers(0).url(), truncated_url);
                ASSERT_EQ(event.iframe_urls_size(), 1);
                EXPECT_EQ(event.iframe_urls(0), truncated_url);
                break;
              }
              case Event::kInterstitialEvent: {
                const auto& event = reported_event.interstitial_event();
                EXPECT_EQ(event.url(), truncated_url);
                ASSERT_EQ(event.referral_urls_size(), 1);
                EXPECT_EQ(event.referral_urls(0), truncated_url);
                EXPECT_EQ(event.url_info().url(), truncated_url);
                ASSERT_EQ(event.referrers_size(), 1);
                EXPECT_EQ(event.referrers(0).url(), truncated_url);
                break;
              }
              case Event::kPasswordReuseEvent: {
                const auto& event = reported_event.password_reuse_event();
                EXPECT_EQ(event.url(), truncated_url);
                ASSERT_EQ(event.referral_urls_size(), 1);
                EXPECT_EQ(event.referral_urls(0), truncated_url);
                break;
              }
              default:
                break;
            }
            std::move(callback).Run(
                policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
            run_loop.Quit();
          });
  reporting_client->ReportEvent(std::move(event_variant), std::move(settings));
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    AllEventTypes,
    RealtimeReportingClientUrlTruncationTest,
    testing::Values(Event::kDangerousDownloadEvent,
                    Event::kUrlFilteringInterstitialEvent,
                    Event::kPasswordBreachEvent,
                    Event::kLoginEvent,
                    Event::kUnscannedFileEvent,
                    Event::kSensitiveDataEvent,
                    Event::kInterstitialEvent,
                    Event::kPasswordReuseEvent),
    [](const testing::TestParamInfo<Event::EventCase>& info) {
      switch (info.param) {
        case Event::kDangerousDownloadEvent:
          return "DangerousDownloadEvent";
        case Event::kUrlFilteringInterstitialEvent:
          return "UrlFilteringInterstitialEvent";
        case Event::kPasswordBreachEvent:
          return "PasswordBreachEvent";
        case Event::kLoginEvent:
          return "LoginEvent";
        case Event::kUnscannedFileEvent:
          return "UnscannedFileEvent";
        case Event::kSensitiveDataEvent:
          return "SensitiveDataEvent";
        case Event::kInterstitialEvent:
          return "InterstitialEvent";
        case Event::kPasswordReuseEvent:
          return "PasswordReuseEvent";
        default:
          return "UnknownEvent";
      }
    });

}  // namespace enterprise_connectors
