// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_reporting_delegate_factory_desktop.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/reporting_features.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"
#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

#include "chrome/browser/profiles/reporting_util.h"

namespace enterprise_reporting {

#if !BUILDFLAG(IS_CHROMEOS)
class SaasUsageBrowserLevelTest : public policy::PolicyTest {
 public:
  SaasUsageBrowserLevelTest() = default;
  ~SaasUsageBrowserLevelTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

    policy::PolicyMap policies;
    base::ListValue domains;
    domains.Append("example.com");
    policies.Set(policy::key::kSaasUsageReportingDomainUrlsForBrowsers,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(domains.Clone()),
                 nullptr);
    provider_.UpdateChromePolicy(policies);

    fake_browser_dm_token_storage_ =
        std::make_unique<policy::FakeBrowserDMTokenStorage>();
    fake_browser_dm_token_storage_->SetClientId("browser_client_id");
    fake_browser_dm_token_storage_->EnableStorage(true);
    fake_browser_dm_token_storage_->SetDMToken("browser_dm_token");
    policy::BrowserDMTokenStorage::SetForTesting(
        fake_browser_dm_token_storage_.get());
  }

  void TearDownOnMainThread() override {
    if (client_) {
      for (auto* loaded_profile :
           g_browser_process->profile_manager()->GetLoadedProfiles()) {
        enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
            loaded_profile)
            ->SetBrowserCloudPolicyClientForTesting(nullptr);
      }
      client_.reset();
    }
    policy::PolicyTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  std::unique_ptr<policy::FakeBrowserDMTokenStorage>
      fake_browser_dm_token_storage_;
};

IN_PROC_BROWSER_TEST_F(SaasUsageBrowserLevelTest, RecordsUsage) {
  client_ = std::make_unique<policy::MockCloudPolicyClient>();
  client_->SetDMToken("browser_dm_token");

  for (auto* loaded_profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        loaded_profile)
        ->SetBrowserCloudPolicyClientForTesting(client_.get());
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("example.com", "/empty.html");

  // Navigate to the domain specified in the policy.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Verify that the usage was recorded in the correct pref service.
  PrefService* pref_service = g_browser_process->local_state();
  ASSERT_TRUE(pref_service);

  const base::DictValue& report_dict = pref_service->GetDict(kSaasUsageReport);

  const base::DictValue* domain_entry = report_dict.FindDict("example.com");
  ASSERT_TRUE(domain_entry);

  // Verify that the basic fields are populated.
  EXPECT_TRUE(domain_entry->Find("navigation_count") != nullptr);

  // Set up expectation for the report upload.
  base::RunLoop run_loop;
  EXPECT_CALL(*client_, UploadSecurityEvent(testing::_, testing::_, testing::_))
      .WillOnce(testing::WithArgs<1, 2>(
          [&run_loop](
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
            EXPECT_EQ(request.events_size(), 1);
            const auto& event = request.events(0);
            EXPECT_TRUE(event.has_saas_usage_report_event());
            const auto& saas_event = event.saas_usage_report_event();
            EXPECT_EQ(saas_event.domain_metrics_size(), 1);
            EXPECT_EQ(saas_event.domain_metrics(0).domain(), "example.com");
            EXPECT_EQ(saas_event.domain_metrics(0).visit_count(), 1u);
            EXPECT_GT(saas_event.domain_metrics(0).start_time_millis(), 0u);
            EXPECT_GT(saas_event.domain_metrics(0).end_time_millis(), 0u);

            std::move(callback).Run(
                policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
            run_loop.Quit();
          }));

  // Force trigger the report upload.
  auto delegate_factory = enterprise_reporting::
      SaasUsageReportingDelegateFactoryDesktop::CreateForBrowser();
  auto scheduler = enterprise_reporting::SaasUsageReportScheduler::Create(
      "browser", delegate_factory.get());
  ASSERT_TRUE(scheduler);
  scheduler->TriggerReport();

  run_loop.Run();
}
#endif

class SaasUsageProfileLevelTest : public policy::PolicyTest {
 public:
  SaasUsageProfileLevelTest() = default;
  ~SaasUsageProfileLevelTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

    policy::PolicyMap policies;
    base::ListValue domains;
    domains.Append("example.com");
    policies.Set(policy::key::kSaasUsageReportingDomainUrlsForProfiles,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(domains.Clone()),
                 nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  void TearDownInProcessBrowserTestFixture() override {
    policy::PolicyTest::TearDownInProcessBrowserTestFixture();
  }

  void TearDownOnMainThread() override {
    if (client_) {
      for (auto* loaded_profile :
           g_browser_process->profile_manager()->GetLoadedProfiles()) {
        enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
            loaded_profile)
            ->SetProfileCloudPolicyClientForTesting(nullptr);
      }
      client_.reset();
    }
    policy::PolicyTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
};

IN_PROC_BROWSER_TEST_F(SaasUsageProfileLevelTest, RecordsUsage) {
  auto* policy_manager = browser()->GetProfile()->GetCloudPolicyManager();
  ASSERT_TRUE(policy_manager);

  enterprise_management::PolicyData policy_data;
  policy_data.set_request_token("profile_dm_token");
  policy_data.set_device_id("client-id");
  policy_manager->core()->store()->set_policy_data_for_testing(
      std::make_unique<enterprise_management::PolicyData>(policy_data));

  client_ = std::make_unique<policy::MockCloudPolicyClient>();
  client_->SetDMToken("profile_dm_token");

  for (auto* loaded_profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        loaded_profile)
        ->SetProfileCloudPolicyClientForTesting(client_.get());
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("example.com", "/empty.html");

  // Navigate to the domain specified in the policy.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Verify that the usage was recorded in the correct pref service.
  PrefService* pref_service = browser()->GetProfile()->GetPrefs();
  ASSERT_TRUE(pref_service);

  const base::DictValue& report_dict = pref_service->GetDict(kSaasUsageReport);

  const base::DictValue* domain_entry = report_dict.FindDict("example.com");
  ASSERT_TRUE(domain_entry);

  // Verify that the basic fields are populated.
  EXPECT_TRUE(domain_entry->Find("navigation_count") != nullptr);

  // Set up expectation for the report upload.
  base::RunLoop run_loop;
  EXPECT_CALL(*client_, UploadSecurityEvent(testing::_, testing::_, testing::_))
      .WillOnce(testing::WithArgs<1, 2>(
          [&run_loop](
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
            EXPECT_EQ(request.events_size(), 1);
            const auto& event = request.events(0);
            EXPECT_TRUE(event.has_saas_usage_report_event());
            const auto& saas_event = event.saas_usage_report_event();
            EXPECT_EQ(saas_event.domain_metrics_size(), 1);
            EXPECT_EQ(saas_event.domain_metrics(0).domain(), "example.com");
            EXPECT_EQ(saas_event.domain_metrics(0).visit_count(), 1u);
            EXPECT_GT(saas_event.domain_metrics(0).start_time_millis(), 0u);
            EXPECT_GT(saas_event.domain_metrics(0).end_time_millis(), 0u);

            std::move(callback).Run(
                policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
            run_loop.Quit();
          }));

  // Force trigger the report upload.
  auto delegate_factory =
      enterprise_reporting::SaasUsageReportingDelegateFactoryDesktop::
          CreateForProfile(browser()->GetProfile());
  auto scheduler = enterprise_reporting::SaasUsageReportScheduler::Create(
      "profile", delegate_factory.get());
  ASSERT_TRUE(scheduler);
  scheduler->TriggerReport();

  run_loop.Run();
}

}  // namespace enterprise_reporting
