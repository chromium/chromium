// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/dm_token_utils.h"
#endif

namespace enterprise_reporting_private =
    ::extensions::api::enterprise_reporting_private;

namespace extensions {

namespace {

constexpr char kBrowserID1[] = "browser_id_1";
constexpr char kBrowserID2[] = "browser_id_2";
constexpr char kProfileID1[] = "profile_id_1";
constexpr char kProfileID2[] = "profile_id_2";

constexpr char kGoogleServiceProvider[] = R"({
      "service_provider": "google",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }
      ]
    })";

constexpr char kOtherServiceProvider[] = R"({
      "service_provider": "other",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }
      ]
    })";

constexpr char kAnotherServiceProvider[] = R"({
      "service_provider": "another",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }
      ]
    })";

}  // namespace

// Base class for non-parametrized GetContextInfo test cases. This class enables
// test cases that would otherwise require a ton of setup in order to be unit
// tests.
class EnterpriseReportingPrivateGetContextInfoBaseBrowserTest
    : public InProcessBrowserTest {
 public:
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableChromeBrowserCloudManagement);
  }
#endif

  void SetupDMToken() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("dm_token"));
#else
    browser_dm_token_storage_ =
        std::make_unique<policy::FakeBrowserDMTokenStorage>();
    browser_dm_token_storage_->SetEnrollmentToken("enrollment_token");
    browser_dm_token_storage_->SetClientId("id");
    browser_dm_token_storage_->SetDMToken("dm_token");
    policy::BrowserDMTokenStorage::SetForTesting(
        browser_dm_token_storage_.get());
#endif
  }

 private:
  std::unique_ptr<policy::FakeBrowserDMTokenStorage> browser_dm_token_storage_;
};

// This browser test class is used to avoid mocking too much browser/profile
// management objects in order to keep the test simple and useful. Please add
// new tests for the getContextInfo API in enterprise_reporting_private_unittest
// and only add tests in this file if they have similar constraints.
class EnterpriseReportingPrivateGetContextInfoBrowserTest
    : public EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
      public testing::WithParamInterface<testing::tuple<bool, bool>> {
 public:
  EnterpriseReportingPrivateGetContextInfoBrowserTest() {
    if (browser_managed()) {
      SetupDMToken();
    }
  }

  bool browser_managed() const { return testing::get<0>(GetParam()); }

  bool profile_managed() const { return testing::get<1>(GetParam()); }

  void SetUpOnMainThread() override {
    EnterpriseReportingPrivateGetContextInfoBaseBrowserTest::
        SetUpOnMainThread();

    if (browser_managed()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      auto* browser_policy_manager = g_browser_process->platform_part()
                                         ->browser_policy_connector_chromeos()
                                         ->GetDeviceCloudPolicyManager();
#else
      auto* browser_policy_manager =
          g_browser_process->browser_policy_connector()
              ->machine_level_user_cloud_policy_manager();
#endif
      auto browser_policy_data =
          std::make_unique<enterprise_management::PolicyData>();
      browser_policy_data->add_device_affiliation_ids(kBrowserID1);
      browser_policy_data->add_device_affiliation_ids(kBrowserID2);
      browser_policy_manager->core()->store()->set_policy_data_for_testing(
          std::move(browser_policy_data));
    }

    if (profile_managed()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      auto* profile_policy_manager =
          browser()->profile()->GetUserCloudPolicyManagerChromeOS();
      profile_policy_manager->core()->client()->SetupRegistration(
          "dm_token", "client_id", {});
#else
      safe_browsing::SetProfileDMToken(browser()->profile(), "dm_token");
      auto* profile_policy_manager =
          browser()->profile()->GetUserCloudPolicyManager();
#endif
      auto profile_policy_data =
          std::make_unique<enterprise_management::PolicyData>();
      profile_policy_data->add_user_affiliation_ids(kProfileID1);
      profile_policy_data->add_user_affiliation_ids(kProfileID2);
      profile_policy_manager->core()->store()->set_policy_data_for_testing(
          std::move(profile_policy_data));
    }
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseReportingPrivateGetContextInfoBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(EnterpriseReportingPrivateGetContextInfoBrowserTest,
                       AffiliationIDs) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = std::unique_ptr<base::Value>(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(),
          /*args*/ "[]", browser()));
  ASSERT_TRUE(context_info_value.get());

  enterprise_reporting_private::ContextInfo info;
  ASSERT_TRUE(enterprise_reporting_private::ContextInfo::Populate(
      *context_info_value, &info));

  if (profile_managed()) {
    EXPECT_EQ(2u, info.profile_affiliation_ids.size());
    EXPECT_EQ(kProfileID1, info.profile_affiliation_ids[0]);
    EXPECT_EQ(kProfileID2, info.profile_affiliation_ids[1]);
  } else {
    EXPECT_TRUE(info.profile_affiliation_ids.empty());
  }

  if (browser_managed()) {
    EXPECT_EQ(2u, info.browser_affiliation_ids.size());
    EXPECT_EQ(kBrowserID1, info.browser_affiliation_ids[0]);
    EXPECT_EQ(kBrowserID2, info.browser_affiliation_ids[1]);
  } else {
    EXPECT_TRUE(info.browser_affiliation_ids.empty());
  }

  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestFileAttachedProviderName) {
  SetupDMToken();
  safe_browsing::SetAnalysisConnector(browser()->profile()->GetPrefs(),
                                      enterprise_connectors::FILE_ATTACHED,
                                      kGoogleServiceProvider);

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = std::unique_ptr<base::Value>(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(),
          /*args*/ "[]", browser()));
  ASSERT_TRUE(context_info_value.get());

  enterprise_reporting_private::ContextInfo info;
  ASSERT_TRUE(enterprise_reporting_private::ContextInfo::Populate(
      *context_info_value, &info));

  EXPECT_EQ(0UL, info.on_file_downloaded_providers.size());
  EXPECT_EQ(0UL, info.on_bulk_data_entry_providers.size());

  EXPECT_EQ(1UL, info.on_file_attached_providers.size());
  EXPECT_EQ("google", info.on_file_attached_providers[0]);
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestFileDownloadedProviderName) {
  SetupDMToken();
  safe_browsing::SetAnalysisConnector(browser()->profile()->GetPrefs(),
                                      enterprise_connectors::FILE_DOWNLOADED,
                                      kGoogleServiceProvider);

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = std::unique_ptr<base::Value>(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(),
          /*args*/ "[]", browser()));
  ASSERT_TRUE(context_info_value.get());

  enterprise_reporting_private::ContextInfo info;
  ASSERT_TRUE(enterprise_reporting_private::ContextInfo::Populate(
      *context_info_value, &info));

  EXPECT_EQ(0UL, info.on_file_attached_providers.size());
  EXPECT_EQ(0UL, info.on_bulk_data_entry_providers.size());

  EXPECT_EQ(1UL, info.on_file_downloaded_providers.size());
  EXPECT_EQ("google", info.on_file_downloaded_providers[0]);
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestBulkDataEntryProviderName) {
  SetupDMToken();
  safe_browsing::SetAnalysisConnector(browser()->profile()->GetPrefs(),
                                      enterprise_connectors::BULK_DATA_ENTRY,
                                      kGoogleServiceProvider);

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = std::unique_ptr<base::Value>(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(),
          /*args*/ "[]", browser()));
  ASSERT_TRUE(context_info_value.get());

  enterprise_reporting_private::ContextInfo info;
  ASSERT_TRUE(enterprise_reporting_private::ContextInfo::Populate(
      *context_info_value, &info));

  EXPECT_EQ(0UL, info.on_file_downloaded_providers.size());
  EXPECT_EQ(0UL, info.on_file_attached_providers.size());

  EXPECT_EQ(1UL, info.on_bulk_data_entry_providers.size());
  EXPECT_EQ("google", info.on_bulk_data_entry_providers[0]);
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestAllProviderNamesSet) {
  SetupDMToken();
  safe_browsing::SetAnalysisConnector(browser()->profile()->GetPrefs(),
                                      enterprise_connectors::BULK_DATA_ENTRY,
                                      kGoogleServiceProvider);
  safe_browsing::SetAnalysisConnector(browser()->profile()->GetPrefs(),
                                      enterprise_connectors::FILE_ATTACHED,
                                      kOtherServiceProvider);
  safe_browsing::SetAnalysisConnector(browser()->profile()->GetPrefs(),
                                      enterprise_connectors::FILE_DOWNLOADED,
                                      kAnotherServiceProvider);

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = std::unique_ptr<base::Value>(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(),
          /*args*/ "[]", browser()));
  ASSERT_TRUE(context_info_value.get());

  enterprise_reporting_private::ContextInfo info;
  ASSERT_TRUE(enterprise_reporting_private::ContextInfo::Populate(
      *context_info_value, &info));

  EXPECT_EQ(1UL, info.on_bulk_data_entry_providers.size());
  EXPECT_EQ("google", info.on_bulk_data_entry_providers[0]);

  EXPECT_EQ(1UL, info.on_file_attached_providers.size());
  EXPECT_EQ("other", info.on_file_attached_providers[0]);

  EXPECT_EQ(1UL, info.on_file_downloaded_providers.size());
  EXPECT_EQ("another", info.on_file_downloaded_providers[0]);
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestOnSecurityEventProviderNameUnset) {
  SetupDMToken();

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = std::unique_ptr<base::Value>(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(),
          /*args*/ "[]", browser()));
  ASSERT_TRUE(context_info_value.get());

  enterprise_reporting_private::ContextInfo info;
  ASSERT_TRUE(enterprise_reporting_private::ContextInfo::Populate(
      *context_info_value, &info));

  EXPECT_EQ(0UL, info.on_security_event_providers.size());
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestOnSecurityEventProviderNameSet) {
  SetupDMToken();
  safe_browsing::SetOnSecurityEventReporting(browser()->profile()->GetPrefs(),
                                             /* enabled= */ true);

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = std::unique_ptr<base::Value>(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(),
          /*args*/ "[]", browser()));
  ASSERT_TRUE(context_info_value.get());

  enterprise_reporting_private::ContextInfo info;
  ASSERT_TRUE(enterprise_reporting_private::ContextInfo::Populate(
      *context_info_value, &info));

  EXPECT_EQ(1UL, info.on_security_event_providers.size());
  // SetOnSecurityEventReporting sets the provider name to google
  EXPECT_EQ("google", info.on_security_event_providers[0]);
}

}  // namespace extensions
