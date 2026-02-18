// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_report_uploader_desktop.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

namespace {

using ::testing::_;

class MockRealtimeReportingClient
    : public enterprise_connectors::RealtimeReportingClient {
 public:
  explicit MockRealtimeReportingClient(content::BrowserContext* context)
      : enterprise_connectors::RealtimeReportingClient(context) {}

  MOCK_METHOD(void,
              ReportSaasUsageEvent,
              (::chrome::cros::reporting::proto::Event event,
               bool is_per_profile,
               const std::string& dm_token,
               base::OnceCallback<void(bool)> upload_callback),
              (override));
};

}  // namespace

class SaasUsageReportUploaderDesktopTest : public testing::Test {
 public:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    fake_browser_dm_token_storage_ =
        std::make_unique<policy::FakeBrowserDMTokenStorage>();
    policy::BrowserDMTokenStorage::SetForTesting(
        fake_browser_dm_token_storage_.get());
  }

  void SetBrowserManaged(bool is_managed) {
    if (is_managed) {
      fake_browser_dm_token_storage_->SetDMToken("browser_dm_token");
      fake_browser_dm_token_storage_->SetClientId("browser_client_id");
    }
  }

  void CreateProfile(bool is_managed,
                     bool is_affiliated,
                     bool create_reporting_client) {
    TestingProfile::Builder builder;

    if (is_managed) {
      auto store = std::make_unique<policy::MockUserCloudPolicyStore>(
          policy::dm_protocol::GetChromeUserPolicyType());
      auto policy_data = std::make_unique<enterprise_management::PolicyData>();
      policy_data->set_request_token("user_dm_token");
      store->set_policy_data_for_testing(std::move(policy_data));

      auto manager = std::make_unique<policy::UserCloudPolicyManager>(
          std::move(store),
          /*extension_install_store=*/nullptr, base::FilePath(),
          std::make_unique<
              testing::NiceMock<policy::MockCloudExternalDataManager>>(),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindRepeating([]() -> network::NetworkConnectionTracker* {
            return network::TestNetworkConnectionTracker::GetInstance();
          }));
      builder.SetUserCloudPolicyManager(std::move(manager));
    }

    builder.SetProfileName("test_profile");
    auto profile = builder.Build();
    profile_ = profile.get();

    if (is_affiliated) {
      profile_->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting(
          {"affiliation_id"});
      TestingBrowserProcess::GetGlobal()
          ->browser_policy_connector()
          ->SetDeviceAffiliatedIdsForTesting({"affiliation_id"});
    }

    // Register the profile with the ProfileManager so that it can be found by
    // the uploader.
    profile_manager_->profile_manager()->RegisterTestingProfile(
        std::move(profile), /*add_to_storage=*/true);

    // Register the mock factory for the RealtimeReportingClient.
    if (create_reporting_client) {
      enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
          ->SetTestingFactory(
              profile_,
              base::BindRepeating(&SaasUsageReportUploaderDesktopTest::
                                      BuildMockRealtimeReportingClient,
                                  base::Unretained(this)));
    }
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()
        ->browser_policy_connector()
        ->SetDeviceAffiliatedIdsForTesting({});
  }

  // Factory function to create the mock client.
  std::unique_ptr<KeyedService> BuildMockRealtimeReportingClient(
      content::BrowserContext* context) {
    return std::make_unique<MockRealtimeReportingClient>(context);
  }

  MockRealtimeReportingClient* GetMockClient() {
    return static_cast<MockRealtimeReportingClient*>(
        enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
            profile_));
  }

  ::chrome::cros::reporting::proto::SaasUsageReportEvent BuildReportEvent() {
    ::chrome::cros::reporting::proto::SaasUsageReportEvent report;
    auto* domain_metrics = report.add_domain_metrics();
    domain_metrics->set_domain("example.com");
    domain_metrics->set_visit_count(1);
    domain_metrics->set_start_time_millis(1000);
    domain_metrics->set_end_time_millis(2000);
    domain_metrics->add_encryption_protocols("TLS 1.3");
    return report;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<policy::FakeBrowserDMTokenStorage>
      fake_browser_dm_token_storage_;
  raw_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

struct SaasUsageReportUploaderDesktopTestParam {
  std::string test_name;
  bool is_browser_managed;
  bool is_profile_managed;
  bool is_affiliated;
  bool is_profile_report_uploader;
  bool feature_enabled;
  bool create_reporting_client;
  std::string expected_dm_token;
  bool expected_per_profile;
  bool expect_report_upload;
};

class SaasUsageReportUploaderDesktopParamTest
    : public SaasUsageReportUploaderDesktopTest,
      public testing::WithParamInterface<
          SaasUsageReportUploaderDesktopTestParam> {
 public:
  void SetUp() override {
    if (GetParam().feature_enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          policy::kUploadRealtimeReportingEventsUsingProto);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          policy::kUploadRealtimeReportingEventsUsingProto);
    }
    SaasUsageReportUploaderDesktopTest::SetUp();
  }
};

TEST_P(SaasUsageReportUploaderDesktopParamTest, UploadReport) {
  const auto& param = GetParam();
  SetBrowserManaged(param.is_browser_managed);
  CreateProfile(param.is_profile_managed, param.is_affiliated,
                param.create_reporting_client);

  // Verify ReportEvent is called with the expected settings.
  if (param.expect_report_upload) {
    EXPECT_CALL(*GetMockClient(), ReportSaasUsageEvent(_, _, _, _))
        .WillOnce([&param](auto event, bool per_profile, std::string dm_token,
                           auto callback) {
          EXPECT_EQ(dm_token, param.expected_dm_token);
          EXPECT_EQ(per_profile, param.expected_per_profile);
        });
  } else if (param.create_reporting_client) {
    EXPECT_CALL(*GetMockClient(), ReportSaasUsageEvent(_, _, _, _)).Times(0);
  }

  std::unique_ptr<SaasUsageReportUploaderDesktop> uploader;
  if (param.is_profile_report_uploader) {
    uploader =
        std::make_unique<SaasUsageProfileReportUploaderDesktop>(profile_);
  } else {
    uploader = std::make_unique<SaasUsageBrowserReportUploaderDesktop>();
  }
  uploader->UploadReport(BuildReportEvent(), base::DoNothing());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SaasUsageReportUploaderDesktopParamTest,
    testing::Values(
        SaasUsageReportUploaderDesktopTestParam{
            .test_name = "UploadBrowserReport_UnmanagedProfile",
            .is_browser_managed = true,
            .is_profile_managed = false,
            .is_affiliated = false,
            .is_profile_report_uploader = false,
            .feature_enabled = true,
            .create_reporting_client = true,
            .expected_dm_token = "browser_dm_token",
            .expected_per_profile = false,
            .expect_report_upload = true},
        SaasUsageReportUploaderDesktopTestParam{
            .test_name = "UploadBrowserReport_ManagedProfile",
            .is_browser_managed = true,
            .is_profile_managed = true,
            .is_affiliated = false,
            .is_profile_report_uploader = false,
            .feature_enabled = true,
            .create_reporting_client = true,
            .expected_dm_token = "browser_dm_token",
            .expected_per_profile = false,
            .expect_report_upload = true},
        SaasUsageReportUploaderDesktopTestParam{
            .test_name = "UploadProfileReport_Unaffiliated",
            .is_browser_managed = true,
            .is_profile_managed = true,
            .is_affiliated = false,
            .is_profile_report_uploader = true,
            .feature_enabled = true,
            .create_reporting_client = true,
            .expected_dm_token = "user_dm_token",
            .expected_per_profile = true,
            .expect_report_upload = true},
        SaasUsageReportUploaderDesktopTestParam{
            .test_name = "UploadProfileReport_Affiliated",
            .is_browser_managed = true,
            .is_profile_managed = true,
            .is_affiliated = true,
            .is_profile_report_uploader = true,
            .feature_enabled = true,
            .create_reporting_client = true,
            .expected_dm_token = "browser_dm_token",
            .expected_per_profile = false,
            .expect_report_upload = true},
        SaasUsageReportUploaderDesktopTestParam{
            .test_name = "UploadBrowserReport_FeatureDisabled",
            .is_browser_managed = true,
            .is_profile_managed = false,
            .is_affiliated = false,
            .is_profile_report_uploader = false,
            .feature_enabled = false,
            .create_reporting_client = true,
            .expected_dm_token = "",
            .expected_per_profile = false,
            .expect_report_upload = false},
        SaasUsageReportUploaderDesktopTestParam{
            .test_name = "UploadBrowserReport_NoReportingClient",
            .is_browser_managed = true,
            .is_profile_managed = false,
            .is_affiliated = false,
            .is_profile_report_uploader = false,
            .feature_enabled = true,
            .create_reporting_client = false,
            .expected_dm_token = "",
            .expected_per_profile = false,
            .expect_report_upload = false},
        SaasUsageReportUploaderDesktopTestParam{
            .test_name = "UploadBrowserReport_NoDMToken",
            .is_browser_managed = false,
            .is_profile_managed = false,
            .is_affiliated = false,
            .is_profile_report_uploader = false,
            .feature_enabled = true,
            .create_reporting_client = true,
            .expected_dm_token = "",
            .expected_per_profile = false,
            .expect_report_upload = false}),
    [](const testing::TestParamInfo<
        SaasUsageReportUploaderDesktopParamTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace enterprise_reporting
