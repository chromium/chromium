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
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/enterprise/browser/enterprise_switches.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#else
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/mgs/managed_guest_session_utils.h"
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
  raw_ptr<extensions::TestEventRouter> event_router_ = nullptr;
};

// Tests to make sure the feature flag and policy control real-time reporting
// as expected.  The parameter for these tests is a tuple of bools:
//
//   bool: whether the feature flag is enabled.
//   bool: whether the session is public or not.
class RealtimeReportingClientIsRealtimeReportingEnabledTest
    : public RealtimeReportingClientTestBase,
      public testing::WithParamInterface<testing::tuple<bool, bool>> {
 public:
  RealtimeReportingClientIsRealtimeReportingEnabledTest()
      : is_feature_flag_enabled_(testing::get<0>(GetParam())),
        is_public_session_(testing::get<1>(GetParam())) {
    if (is_feature_flag_enabled_) {
      scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabledOnMGS},
                                            {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {kEnterpriseConnectorsEnabledOnMGS});
    }

    // In chrome branded desktop builds, the browser is always manageable.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableChromeBrowserCloudManagement);
#endif
  }

  void SetUp() override {
    RealtimeReportingClientTestBase::SetUp();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    const AccountId account_id(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));
    const user_manager::User* user;
    if (is_public_session_) {
      user = user_manager->AddPublicAccountUser(account_id);
    } else {
      user = user_manager->AddUserWithAffiliation(account_id,
                                                  /*is_affiliated=*/true);
    }
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                 profile_);
    user_manager->UserLoggedIn(account_id, user->username_hash(),
                               /*browser_restart=*/false,
                               /*is_child=*/false);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    profile_->ScopedCrosSettingsTestHelper()
        ->InstallAttributes()
        ->SetCloudManaged("domain.com", "device_id");
#endif
  }

  bool should_init() {
    bool is_mgs = false;
#if BUILDFLAG(IS_CHROMEOS)
    is_mgs = chromeos::IsManagedGuestSession();
#endif
    return is_feature_flag_enabled_ || !is_mgs;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  const bool is_feature_flag_enabled_;
  const bool is_public_session_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
 private:
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif
};

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

}  // namespace

TEST_P(RealtimeReportingClientIsRealtimeReportingEnabledTest,
       ShouldInitRealtimeReportingClient) {
  EXPECT_EQ(should_init(),
            RealtimeReportingClient::ShouldInitRealtimeReportingClient());
}

INSTANTIATE_TEST_SUITE_P(All,
                         RealtimeReportingClientIsRealtimeReportingEnabledTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class RealtimeReportingClientUmaTest : public RealtimeReportingClientTestBase {
 public:
  void SetUp() override {
    RealtimeReportingClientTestBase::SetUp();
    reporting_client_ =
        enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
            profile_);
    reporting_client_->SetBrowserCloudPolicyClientForTesting(client_.get());
  }

 protected:
  base::HistogramTester histogram_;
  raw_ptr<RealtimeReportingClient> reporting_client_ = nullptr;
  policy::CloudPolicyClient::ResultCallback upload_callback;
};

TEST_F(RealtimeReportingClientUmaTest, TestUmaEventUploadSucceeds) {
  ReportingSettings settings;
  base::Value::Dict event;

  EXPECT_CALL(*client_.get(), UploadSecurityEventReport(_, _, _))
      .WillOnce(MoveArg<2>(&upload_callback));

  reporting_client_->ReportRealtimeEvent(kExtensionInstallEvent,
                                         std::move(settings), std::move(event));

  std::move(upload_callback)
      .Run(policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));

  histogram_.ExpectUniqueSample(
      "Enterprise.ReportingEventUploadSuccess",
      EnterpriseReportingEventType::kExtensionInstallEvent, 1);
  histogram_.ExpectTotalCount("Enterprise.ReportingEventUploadFailure", 0);
}

TEST_F(RealtimeReportingClientUmaTest, TestUmaEventUploadFails) {
  ReportingSettings settings;
  base::Value::Dict event;

  EXPECT_CALL(*client_.get(), UploadSecurityEventReport(_, _, _))
      .WillOnce(MoveArg<2>(&upload_callback));

  reporting_client_->ReportRealtimeEvent(kExtensionInstallEvent,
                                         std::move(settings), std::move(event));

  std::move(upload_callback)
      .Run(policy::CloudPolicyClient::Result(policy::DM_STATUS_REQUEST_FAILED));

  histogram_.ExpectUniqueSample(
      "Enterprise.ReportingEventUploadFailure",
      EnterpriseReportingEventType::kExtensionInstallEvent, 1);
  histogram_.ExpectTotalCount("Enterprise.ReportingEventUploadSuccess", 0);
}

TEST_F(RealtimeReportingClientTestBase,
       TestEventNameToUmaEnumMapIncludesAllEvents) {
  EXPECT_EQ(sizeof(kAllReportingEvents) / sizeof(kAllReportingEvents[0]),
            kEventNameToUmaEnumMap.size());
  for (const char* eventName : kAllReportingEvents) {
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
