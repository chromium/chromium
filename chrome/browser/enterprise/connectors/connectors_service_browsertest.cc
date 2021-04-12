// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_service.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_browsertest_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_test.h"

namespace enterprise_connectors {

namespace {

constexpr char kNormalAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp", "malware"]}
    ]
  }
])";

constexpr char kNormalReportingSettingsPref[] = R"([
  {
    "service_provider": "google"
  }
])";

#if !BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kFakeProfileDMToken[] = "fake-profile-dm-token";
constexpr char kFakeEnrollmentToken[] = "fake-enrollment-token";
constexpr char kFakeBrowserClientId[] = "fake-browser-client-id";
constexpr char kFakeProfileClientId[] = "fake-profile-client-id";
constexpr char kAffiliationId1[] = "affiliation-id-1";
constexpr char kAffiliationId2[] = "affiliation-id-2";
#endif

constexpr char kFakeBrowserDMToken[] = "fake-browser-dm-token";
constexpr char kTestUrl[] = "https://foo.com";

}  // namespace

// Profile DM token tests
// These tests validate that ConnectorsService obtains the correct DM token on
// each GetAnalysisSettings/GetReportingSettings call. There are 3 mains cases
// to validate here:
//
// - Affiliated: The profile and browser are managed by the same customer. In
// this case, it is OK to get the profile DM token and apply Connector policies.
// - Unaffiliated: The profile and browser are managed by different customers.
// In this case, no profile settings should be returned.
// - Unmanaged: The profile is managed by a customer while the browser is
// unmanaged. In this case, it is OK to get the profile DM token and apply
// Connector policies.
//
// The exception to the above rules is CrOS. Even when the policies are applied
// at a user scope, only the browser DM token should be returned.

enum class ManagementStatus { AFFILIATED, UNAFFILIATED, UNMANAGED };

class ConnectorsServiceProfileBrowserTest
    : public safe_browsing::DeepScanningBrowserTestBase {
 public:
  explicit ConnectorsServiceProfileBrowserTest(
      ManagementStatus management_status)
      : management_status_(management_status) {
    if (management_status_ != ManagementStatus::UNMANAGED) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      policy::SetDMTokenForTesting(
          policy::DMToken::CreateValidTokenForTesting(kFakeBrowserDMToken));
#else
      browser_dm_token_storage_ =
          std::make_unique<policy::FakeBrowserDMTokenStorage>();
      browser_dm_token_storage_->SetEnrollmentToken(kFakeEnrollmentToken);
      browser_dm_token_storage_->SetClientId(kFakeBrowserClientId);
      browser_dm_token_storage_->EnableStorage(true);
      browser_dm_token_storage_->SetDMToken(kFakeBrowserDMToken);
      policy::BrowserDMTokenStorage::SetForTesting(
          browser_dm_token_storage_.get());
#endif
    }

    // Set the required features for the per-profile feature to work.
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {kEnterpriseConnectorsEnabled, kPerProfileConnectorsEnabled}, {});
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpOnMainThread() override {
    safe_browsing::DeepScanningBrowserTestBase::SetUpOnMainThread();

    safe_browsing::SetProfileDMToken(browser()->profile(), kFakeProfileDMToken);

    // Set profile/browser affiliation IDs.
    auto* profile_policy_manager =
        browser()->profile()->GetUserCloudPolicyManager();
    auto profile_policy_data =
        std::make_unique<enterprise_management::PolicyData>();
    profile_policy_data->add_user_affiliation_ids(kAffiliationId1);
    profile_policy_data->set_device_id(kFakeProfileClientId);
    profile_policy_manager->core()->store()->set_policy_data_for_testing(
        std::move(profile_policy_data));

    if (management_status_ != ManagementStatus::UNMANAGED) {
      auto* browser_policy_manager =
          g_browser_process->browser_policy_connector()
              ->machine_level_user_cloud_policy_manager();
      auto browser_policy_data =
          std::make_unique<enterprise_management::PolicyData>();
      browser_policy_data->add_device_affiliation_ids(
          management_status() == ManagementStatus::AFFILIATED
              ? kAffiliationId1
              : kAffiliationId2);
      browser_policy_manager->core()->store()->set_policy_data_for_testing(
          std::move(browser_policy_data));
    }
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableChromeBrowserCloudManagement);
  }
#endif

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  void SetPrefs(const char* pref,
                const char* scope_pref,
                const char* pref_value,
                bool profile_scope = true) {
    browser()->profile()->GetPrefs()->Set(pref,
                                          *base::JSONReader::Read(pref_value));
    browser()->profile()->GetPrefs()->SetInteger(
        scope_pref, profile_scope ? policy::POLICY_SCOPE_USER
                                  : policy::POLICY_SCOPE_MACHINE);
  }
  void SetPrefs(const char* pref,
                const char* scope_pref,
                int pref_value,
                bool profile_scope = true) {
    browser()->profile()->GetPrefs()->SetInteger(pref, pref_value);
    browser()->profile()->GetPrefs()->SetInteger(
        scope_pref, profile_scope ? policy::POLICY_SCOPE_USER
                                  : policy::POLICY_SCOPE_MACHINE);
  }

  ManagementStatus management_status() { return management_status_; }

 protected:
  std::unique_ptr<policy::FakeBrowserDMTokenStorage> browser_dm_token_storage_;
  ManagementStatus management_status_;
};

class ConnectorsServiceReportingProfileBrowserTest
    : public ConnectorsServiceProfileBrowserTest,
      public testing::WithParamInterface<
          std::tuple<ReportingConnector, ManagementStatus>> {
 public:
  ConnectorsServiceReportingProfileBrowserTest()
      : ConnectorsServiceProfileBrowserTest(std::get<1>(GetParam())) {}
  ReportingConnector connector() { return std::get<0>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ConnectorsServiceReportingProfileBrowserTest,
    testing::Combine(testing::Values(ReportingConnector::SECURITY_EVENT),
                     testing::Values(ManagementStatus::AFFILIATED,
                                     ManagementStatus::UNAFFILIATED,
                                     ManagementStatus::UNMANAGED)));

IN_PROC_BROWSER_TEST_P(ConnectorsServiceReportingProfileBrowserTest, Test) {
  SetPrefs(ConnectorPref(connector()), ConnectorScopePref(connector()),
           kNormalReportingSettingsPref);

  auto settings =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetReportingSettings(connector());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (management_status() == ManagementStatus::UNMANAGED) {
    ASSERT_FALSE(settings.has_value());
  } else {
    ASSERT_TRUE(settings.has_value());
    ASSERT_FALSE(settings.value().per_profile);
    ASSERT_EQ(kFakeBrowserDMToken, settings.value().dm_token);
  }
#else
  switch (management_status()) {
    case ManagementStatus::AFFILIATED:
      EXPECT_TRUE(settings.has_value());
      ASSERT_EQ(kFakeProfileDMToken, settings.value().dm_token);
      ASSERT_TRUE(settings.value().per_profile);
      break;
    case ManagementStatus::UNAFFILIATED:
      EXPECT_FALSE(settings.has_value());
      break;
    case ManagementStatus::UNMANAGED:
      EXPECT_TRUE(settings.has_value());
      ASSERT_EQ(kFakeProfileDMToken, settings.value().dm_token);
      ASSERT_TRUE(settings.value().per_profile);
      break;
  }
#endif
}

class ConnectorsServiceAnalysisProfileBrowserTest
    : public ConnectorsServiceProfileBrowserTest,
      public testing::WithParamInterface<
          std::tuple<AnalysisConnector, ManagementStatus>> {
 public:
  ConnectorsServiceAnalysisProfileBrowserTest()
      : ConnectorsServiceProfileBrowserTest(std::get<1>(GetParam())) {}
  AnalysisConnector connector() { return std::get<0>(GetParam()); }

  void ValidateClientMetadata(const ClientMetadata& metadata,
                              bool profile_reporting) {
    ASSERT_TRUE(metadata.has_browser());
    ASSERT_TRUE(metadata.browser().has_browser_id());
    ASSERT_TRUE(metadata.browser().has_user_agent());
    ASSERT_TRUE(metadata.browser().has_chrome_version());
    ASSERT_NE(profile_reporting, metadata.browser().has_machine_user());

    ASSERT_NE(profile_reporting, metadata.has_device());
    if (!profile_reporting) {
      // The device DM token should only be populated when reporting is set at
      // the device level, aka not the profile level.
      ASSERT_NE(profile_reporting, metadata.device().has_dm_token());
      ASSERT_TRUE(metadata.device().has_client_id());
      ASSERT_TRUE(metadata.device().has_os_version());
      ASSERT_TRUE(metadata.device().has_os_platform());
      ASSERT_TRUE(metadata.device().has_name());
    }

    ASSERT_TRUE(metadata.has_profile());
    ASSERT_EQ(profile_reporting, metadata.profile().has_dm_token());
    ASSERT_TRUE(metadata.profile().has_gaia_email());
    ASSERT_TRUE(metadata.profile().has_profile_path());
    ASSERT_TRUE(metadata.profile().has_profile_name());
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    ASSERT_TRUE(metadata.profile().has_client_id());
#endif
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ConnectorsServiceAnalysisProfileBrowserTest,
    testing::Combine(
        testing::Values(FILE_ATTACHED, FILE_DOWNLOADED, BULK_DATA_ENTRY),
        testing::Values(ManagementStatus::AFFILIATED,
                        ManagementStatus::UNAFFILIATED,
                        ManagementStatus::UNMANAGED)));

IN_PROC_BROWSER_TEST_P(ConnectorsServiceAnalysisProfileBrowserTest,
                       DeviceReporting) {
  SetPrefs(ConnectorPref(connector()), ConnectorScopePref(connector()),
           kNormalAnalysisSettingsPref, /*profile_scope*/ false);
  SetPrefs(ConnectorPref(ReportingConnector::SECURITY_EVENT),
           ConnectorScopePref(ReportingConnector::SECURITY_EVENT),
           kNormalReportingSettingsPref, /*profile_scope*/ false);
  auto settings =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetAnalysisSettings(GURL(kTestUrl), connector());

  if (management_status() == ManagementStatus::UNMANAGED) {
    ASSERT_FALSE(settings.has_value());
  } else {
    ASSERT_TRUE(settings.has_value());
    ASSERT_EQ(kFakeBrowserDMToken, settings.value().dm_token);
    ASSERT_FALSE(settings.value().per_profile);
    ValidateClientMetadata(*settings.value().client_metadata,
                           /*profile_reporting*/ false);
  }
}

IN_PROC_BROWSER_TEST_P(ConnectorsServiceAnalysisProfileBrowserTest,
                       ProfileReporting) {
  SetPrefs(ConnectorPref(connector()), ConnectorScopePref(connector()),
           kNormalAnalysisSettingsPref);
  SetPrefs(ConnectorPref(ReportingConnector::SECURITY_EVENT),
           ConnectorScopePref(ReportingConnector::SECURITY_EVENT),
           kNormalReportingSettingsPref);
  auto settings =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetAnalysisSettings(GURL(kTestUrl), connector());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (management_status() == ManagementStatus::UNMANAGED) {
    ASSERT_FALSE(settings.has_value());
  } else {
    ASSERT_TRUE(settings.has_value());
    ASSERT_EQ(kFakeBrowserDMToken, settings.value().dm_token);
    ASSERT_FALSE(settings.value().per_profile);
    ValidateClientMetadata(*settings.value().client_metadata,
                           /*profile_reporting*/ false);
  }
#else
  switch (management_status()) {
    case ManagementStatus::AFFILIATED:
      EXPECT_TRUE(settings.has_value());
      ASSERT_EQ(kFakeProfileDMToken, settings.value().dm_token);
      ASSERT_TRUE(settings.value().per_profile);
      ValidateClientMetadata(*settings.value().client_metadata,
                             /*profile_reporting*/ true);
      break;
    case ManagementStatus::UNAFFILIATED:
      EXPECT_FALSE(settings.has_value());
      break;
    case ManagementStatus::UNMANAGED:
      EXPECT_TRUE(settings.has_value());
      ASSERT_EQ(kFakeProfileDMToken, settings.value().dm_token);
      ASSERT_TRUE(settings.value().per_profile);
      ASSERT_TRUE(settings.value().client_metadata);
      ValidateClientMetadata(*settings.value().client_metadata,
                             /*profile_reporting*/ true);
      break;
  }
#endif
}

IN_PROC_BROWSER_TEST_P(ConnectorsServiceAnalysisProfileBrowserTest,
                       NoReporting) {
  SetPrefs(ConnectorPref(connector()), ConnectorScopePref(connector()),
           kNormalAnalysisSettingsPref);
  auto settings =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetAnalysisSettings(GURL(kTestUrl), connector());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (management_status() == ManagementStatus::UNMANAGED) {
    ASSERT_FALSE(settings.has_value());
  } else {
    ASSERT_TRUE(settings.has_value());
    ASSERT_EQ(kFakeBrowserDMToken, settings.value().dm_token);
    ASSERT_FALSE(settings.value().per_profile);
    ASSERT_FALSE(settings.value().client_metadata);
  }
#else
  switch (management_status()) {
    case ManagementStatus::AFFILIATED:
      EXPECT_TRUE(settings.has_value());
      ASSERT_EQ(kFakeProfileDMToken, settings.value().dm_token);
      ASSERT_TRUE(settings.value().per_profile);
      ASSERT_FALSE(settings.value().client_metadata);
      break;
    case ManagementStatus::UNAFFILIATED:
      EXPECT_FALSE(settings.has_value());
      break;
    case ManagementStatus::UNMANAGED:
      EXPECT_TRUE(settings.has_value());
      ASSERT_EQ(kFakeProfileDMToken, settings.value().dm_token);
      ASSERT_TRUE(settings.value().per_profile);
      ASSERT_FALSE(settings.value().client_metadata);
      break;
  }
#endif
}

class ConnectorsServiceRealtimeURLCheckProfileBrowserTest
    : public ConnectorsServiceProfileBrowserTest,
      public testing::WithParamInterface<ManagementStatus> {
 public:
  ConnectorsServiceRealtimeURLCheckProfileBrowserTest()
      : ConnectorsServiceProfileBrowserTest(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         ConnectorsServiceRealtimeURLCheckProfileBrowserTest,
                         testing::Values(ManagementStatus::AFFILIATED,
                                         ManagementStatus::UNAFFILIATED,
                                         ManagementStatus::UNMANAGED));

IN_PROC_BROWSER_TEST_P(ConnectorsServiceRealtimeURLCheckProfileBrowserTest,
                       Test) {
  SetPrefs(prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode,
           prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope, 1);
  auto maybe_dm_token =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetDMTokenForRealTimeUrlCheck();
  safe_browsing::EnterpriseRealTimeUrlCheckMode url_check_pref =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetAppliedRealTimeUrlCheck();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (management_status() == ManagementStatus::UNMANAGED) {
    ASSERT_FALSE(maybe_dm_token.has_value());
    ASSERT_EQ(safe_browsing::REAL_TIME_CHECK_DISABLED, url_check_pref);
  } else {
    ASSERT_TRUE(maybe_dm_token.has_value());
    ASSERT_EQ(kFakeBrowserDMToken, maybe_dm_token.value());
    ASSERT_EQ(safe_browsing::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED,
              url_check_pref);
  }
#else
  switch (management_status()) {
    case ManagementStatus::AFFILIATED:
      ASSERT_TRUE(maybe_dm_token.has_value());
      ASSERT_EQ(kFakeProfileDMToken, maybe_dm_token.value());
      ASSERT_EQ(safe_browsing::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED,
                url_check_pref);
      break;
    case ManagementStatus::UNAFFILIATED:
      ASSERT_FALSE(maybe_dm_token.has_value());
      ASSERT_EQ(safe_browsing::REAL_TIME_CHECK_DISABLED, url_check_pref);
      break;
    case ManagementStatus::UNMANAGED:
      ASSERT_TRUE(maybe_dm_token.has_value());
      ASSERT_EQ(kFakeProfileDMToken, maybe_dm_token.value());
      ASSERT_EQ(safe_browsing::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED,
                url_check_pref);
      break;
  }
#endif
}

// This test validates that no settings are obtained when
// kPerProfileConnectorsEnabled is disabled. CrOS is unaffected as it only gets
// the browser token if it is present.
class ConnectorsServiceNoProfileFeatureBrowserTest
    : public ConnectorsServiceProfileBrowserTest,
      public testing::WithParamInterface<ManagementStatus> {
 public:
  ConnectorsServiceNoProfileFeatureBrowserTest()
      : ConnectorsServiceProfileBrowserTest(GetParam()) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled},
                                          {kPerProfileConnectorsEnabled});
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         ConnectorsServiceNoProfileFeatureBrowserTest,
                         testing::Values(ManagementStatus::AFFILIATED,
                                         ManagementStatus::UNAFFILIATED,
                                         ManagementStatus::UNMANAGED));

IN_PROC_BROWSER_TEST_P(ConnectorsServiceNoProfileFeatureBrowserTest, Test) {
  for (auto connector : {FILE_ATTACHED, FILE_DOWNLOADED, BULK_DATA_ENTRY}) {
    SetPrefs(ConnectorPref(connector), ConnectorScopePref(connector),
             kNormalAnalysisSettingsPref);
  }
  SetPrefs(ConnectorPref(ReportingConnector::SECURITY_EVENT),
           ConnectorScopePref(ReportingConnector::SECURITY_EVENT),
           kNormalReportingSettingsPref);
  SetPrefs(prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode,
           prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope, 1);

  for (auto connector : {FILE_ATTACHED, FILE_DOWNLOADED, BULK_DATA_ENTRY}) {
    auto settings =
        ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
            ->GetAnalysisSettings(GURL(kTestUrl), connector);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (management_status() == ManagementStatus::UNMANAGED) {
      ASSERT_FALSE(settings.has_value());
    } else {
      ASSERT_TRUE(settings.has_value());
      ASSERT_EQ(kFakeBrowserDMToken, settings.value().dm_token);
      ASSERT_FALSE(settings.value().per_profile);
    }
#else
    EXPECT_FALSE(settings.has_value());
#endif
  }

  auto settings =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetReportingSettings(ReportingConnector::SECURITY_EVENT);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (management_status() == ManagementStatus::UNMANAGED) {
    ASSERT_FALSE(settings.has_value());
  } else {
    ASSERT_TRUE(settings.has_value());
    ASSERT_EQ(kFakeBrowserDMToken, settings.value().dm_token);
    ASSERT_FALSE(settings.value().per_profile);
  }
#else
  EXPECT_FALSE(settings.has_value());
#endif

  auto maybe_dm_token =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetDMTokenForRealTimeUrlCheck();
  safe_browsing::EnterpriseRealTimeUrlCheckMode url_check_pref =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetAppliedRealTimeUrlCheck();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(management_status() != ManagementStatus::UNMANAGED,
            maybe_dm_token.has_value());
  if (maybe_dm_token.has_value()) {
    EXPECT_EQ(kFakeBrowserDMToken, maybe_dm_token.value());
    ASSERT_EQ(safe_browsing::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED,
              url_check_pref);
  } else {
    EXPECT_EQ(safe_browsing::REAL_TIME_CHECK_DISABLED, url_check_pref);
  }
#else
  EXPECT_FALSE(maybe_dm_token.has_value());
  EXPECT_EQ(safe_browsing::REAL_TIME_CHECK_DISABLED, url_check_pref);
#endif
}

}  // namespace enterprise_connectors
