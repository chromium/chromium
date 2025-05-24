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
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "components/device_signals/core/browser/signals_types.h"
#endif

using testing::_;

namespace enterprise_connectors {

namespace {

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
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  RealtimeReportingClientUmaTest() {
    if (local_ip_addresses_enabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          safe_browsing::kLocalIpAddressInEvents);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          safe_browsing::kLocalIpAddressInEvents);
    }
  }

  bool is_profile_reporting() { return std::get<0>(GetParam()); }

  bool local_ip_addresses_enabled() { return std::get<1>(GetParam()); }

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
      .WillOnce(testing::Invoke(
          [&](bool include_device_info, base::Value::Dict&& report,
              policy::CloudPolicyClient::ResultCallback callback) {
            upload_callback_ = std::move(callback);
            run_loop.Quit();
          }));
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
      .WillOnce(testing::Invoke(
          [&](bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest&& request,
              policy::CloudPolicyClient::ResultCallback callback) {
            upload_callback_ = std::move(callback);
            run_loop.Quit();
          }));
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
      .WillOnce(testing::Invoke(
          [&](bool include_device_info, base::Value::Dict&& report,
              policy::CloudPolicyClient::ResultCallback callback) {
            upload_callback_ = std::move(callback);
            run_loop.Quit();
          }));
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
      .WillOnce(testing::Invoke(
          [&](bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest&& request,
              policy::CloudPolicyClient::ResultCallback callback) {
            upload_callback_ = std::move(callback);
            run_loop.Quit();
          }));
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

INSTANTIATE_TEST_SUITE_P(
    All,
    RealtimeReportingClientUmaTest,
    testing::Combine(/* is_profile_reporting */ testing::Bool(),
                     /* local_ip_addresses_enabled */ testing::Bool()));

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

#endif
}  // namespace enterprise_connectors
