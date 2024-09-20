// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_service.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_browsertest_base.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/browser/ui/browser.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/reporting_job_configuration_base.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#endif

namespace enterprise_connectors {

namespace {

constexpr char kNormalCloudAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp", "malware"]}
    ]
  }
])";

constexpr char kNormalLocalAnalysisSettingsPref[] = R"([
  {
    "service_provider": "local_user_agent",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp"]}
    ]
  }
])";

constexpr char kNormalReportingSettingsPref[] = R"([
  {
    "service_provider": "google"
  }
])";

#if !BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kAffiliationId2[] = "affiliation-id-2";
#endif

#if !BUILDFLAG(IS_CHROMEOS)
constexpr char kFakeEnrollmentToken[] = "fake-enrollment-token";
constexpr char kUsername1[] = "user@domain1.com";
constexpr char kUsername2[] = "admin@domain2.com";
constexpr char kDomain2[] = "domain2.com";
#endif

constexpr char kFakeBrowserDMToken[] = "fake-browser-dm-token";
constexpr char kFakeProfileDMToken[] = "fake-profile-dm-token";
constexpr char kFakeBrowserClientId[] = "fake-browser-client-id";
constexpr char kFakeProfileClientId[] = "fake-profile-client-id";
constexpr char kAffiliationId1[] = "affiliation-id-1";
constexpr char kDomain1[] = "domain1.com";
constexpr char kTestUrl[] = "https://foo.com";

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kTestGaiaId[] = "123";
constexpr char kTestEmail[] = "test@test";
#endif

std::string ExpectedOsPlatform() {
#if BUILDFLAG(IS_WIN)
  return "Windows";
#elif BUILDFLAG(IS_MAC)
  return "Mac OS X";
#elif BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return "ChromeOS";
#else
  return "ChromiumOS";
#endif
#elif BUILDFLAG(IS_LINUX)
  return "Linux";
#endif
}

// We want a way to check whether or not any metadata was set, but the profile
// metadata will always contain the `is_chrome_os_managed_guest_session` field
// if the `kEnterpriseConnectorsEnabledOnMGS` feature flag is enabled, so we
// check another field (which should always be set if any actual metadata was
// provided).
bool ContainsClientId(const AnalysisSettings& settings) {
  return settings.client_metadata && settings.client_metadata->has_device() &&
         settings.client_metadata->device().has_client_id();
}

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
    : public test::DeepScanningBrowserTestBase {
 public:
  explicit ConnectorsServiceProfileBrowserTest(
      ManagementStatus management_status)
      : management_status_(management_status) {
    if (management_status_ != ManagementStatus::UNMANAGED) {
#if BUILDFLAG(IS_CHROMEOS)
      policy::SetDMTokenForTesting(
          policy::DMToken::CreateValidToken(kFakeBrowserDMToken));
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
  }

  void SetUpOnMainThread() override {
    test::DeepScanningBrowserTestBase::SetUpOnMainThread();

    SetUpProfileData();

    if (management_status_ != ManagementStatus::UNMANAGED) {
      SetUpDeviceData();
    }
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_enabler_.reset();
#endif
  }

  void SetUpProfileData() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    EXPECT_TRUE(browser()->profile()->IsMainProfile());
#elif !BUILDFLAG(IS_CHROMEOS_ASH)
    test::SetProfileDMToken(browser()->profile(), kFakeProfileDMToken);
#endif

    enterprise_management::PolicyData profile_policy_data;
    profile_policy_data.add_user_affiliation_ids(kAffiliationId1);
    profile_policy_data.set_managed_by(kDomain1);
    profile_policy_data.set_device_id(kFakeProfileClientId);
    profile_policy_data.set_request_token(kFakeProfileDMToken);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (management_status_ != ManagementStatus::UNMANAGED) {
      policy::PolicyLoaderLacros::set_main_user_policy_data_for_testing(
          std::move(profile_policy_data));
    }
#else
    auto* profile_policy_manager =
#if BUILDFLAG(IS_CHROMEOS_ASH)
        browser()->profile()->GetUserCloudPolicyManagerAsh();
#else
        browser()->profile()->GetUserCloudPolicyManager();
#endif

    profile_policy_manager->core()->store()->set_policy_data_for_testing(
        std::make_unique<enterprise_management::PolicyData>(
            std::move(profile_policy_data)));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  void SetUpDeviceData() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    crosapi::mojom::BrowserInitParamsPtr init_params =
        crosapi::mojom::BrowserInitParams::New();
    init_params->device_properties = crosapi::mojom::DeviceProperties::New();
    init_params->device_properties->device_dm_token = kFakeBrowserDMToken;
    init_params->device_properties->device_affiliation_ids = {
        management_status() == ManagementStatus::AFFILIATED ? kAffiliationId1
                                                            : kAffiliationId2};
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#elif BUILDFLAG(IS_CHROMEOS_ASH)
    auto* fake_user_manager = new ash::FakeChromeUserManager();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager));
    AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId);
    fake_user_manager->AddUserWithAffiliationAndTypeAndProfile(
        account_id, management_status() == ManagementStatus::AFFILIATED,
        user_manager::UserType::kRegular,
        static_cast<TestingProfile*>(browser()->profile()));
    fake_user_manager->LoginUser(account_id);
#else
    auto* browser_policy_manager =
        g_browser_process->browser_policy_connector()
            ->machine_level_user_cloud_policy_manager();
    auto browser_policy_data =
        std::make_unique<enterprise_management::PolicyData>();
    browser_policy_data->add_device_affiliation_ids(
        management_status() == ManagementStatus::AFFILIATED ? kAffiliationId1
                                                            : kAffiliationId2);
    browser_policy_data->set_username(
        management_status() == ManagementStatus::AFFILIATED ? kUsername1
                                                            : kUsername2);
    browser_policy_manager->core()->store()->set_policy_data_for_testing(
        std::move(browser_policy_data));
#endif
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableChromeBrowserCloudManagement);
  }
#endif

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
 private:
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
#endif
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
  SetPrefs(kOnSecurityEventPref, kOnSecurityEventScopePref,
           kNormalReportingSettingsPref);

  auto settings =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetReportingSettings(connector());
#if BUILDFLAG(IS_CHROMEOS)
  if (management_status() == ManagementStatus::UNMANAGED) {
    ASSERT_FALSE(settings.has_value());
  } else {
    ASSERT_TRUE(settings.has_value());
    ASSERT_FALSE(settings.value().per_profile);
    ASSERT_EQ(kFakeBrowserDMToken, settings.value().dm_token);
  }
#else
  std::string management_domain =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetManagementDomain();
  // expected state is the same regardless of management_status() value.
  EXPECT_TRUE(settings.has_value());
  ASSERT_EQ(kFakeProfileDMToken, settings.value().dm_token);
  ASSERT_TRUE(settings.value().per_profile);
  ASSERT_EQ(kDomain1, management_domain);
#endif
}

class ConnectorsServiceAnalysisProfileBrowserTest
    : public ConnectorsServiceProfileBrowserTest,
      public testing::WithParamInterface<
          std::tuple<ManagementStatus, const char*, bool>> {
 public:
  ConnectorsServiceAnalysisProfileBrowserTest()
      : ConnectorsServiceProfileBrowserTest(std::get<0>(GetParam())) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (enterprise_connectors_enabled_on_mgs()) {
      enabled_features.push_back(kEnterpriseConnectorsEnabledOnMGS);
    } else {
      disabled_features.push_back(kEnterpriseConnectorsEnabledOnMGS);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  const char* settings_value() { return std::get<1>(GetParam()); }

  // Returns whether the kEnterpriseConnectorsEnabledOnMGS feature should be
  // enabled or not.
  bool enterprise_connectors_enabled_on_mgs() {
    return std::get<2>(GetParam());
  }

  bool is_cloud() {
    return strcmp(settings_value(), kNormalCloudAnalysisSettingsPref) == 0;
  }

  // Returns the Value the "normal" reporting workflow uses to validate that it
  // is in sync with the information sent through analysis-reporting.
  base::Value::Dict ReportingMetadata(bool is_cloud, bool include_device_info) {
    base::Value::Dict output;
    output.Set("browser",
               policy::ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                   BuildBrowserDictionary(!is_cloud || include_device_info));
    base::Value::Dict context = reporting::GetContext(browser()->profile());
    output.Merge(std::move(context));
    if (include_device_info) {
      base::Value::Dict device;
      device.Set("device", policy::ReportingJobConfigurationBase::
                               DeviceDictionaryBuilder ::BuildDeviceDictionary(
                                   kFakeBrowserDMToken, kFakeBrowserClientId));
      output.Merge(std::move(device));
    }

    return output;
  }

  void ValidateClientMetadata(const ClientMetadata& metadata,
                              bool is_cloud,
                              bool profile_reporting) {
#if BUILDFLAG(IS_CHROMEOS)
    bool includes_device_info =
        management_status() == ManagementStatus::AFFILIATED;
#else
    bool includes_device_info =
        !profile_reporting ||
        (management_status() == ManagementStatus::AFFILIATED && is_cloud);
#endif
    base::Value::Dict reporting_metadata =
        ReportingMetadata(is_cloud, includes_device_info);

    ASSERT_TRUE(metadata.has_browser());

    ASSERT_TRUE(metadata.browser().has_browser_id());
    ASSERT_EQ(metadata.browser().browser_id(),
              *reporting_metadata.FindStringByDottedPath("browser.browserId"));

    ASSERT_TRUE(metadata.browser().has_user_agent());
    ASSERT_EQ(metadata.browser().user_agent(),
              *reporting_metadata.FindStringByDottedPath("browser.userAgent"));

    ASSERT_TRUE(metadata.browser().has_chrome_version());
    ASSERT_EQ(metadata.browser().chrome_version(),
              version_info::GetVersionNumber());
    ASSERT_EQ(
        metadata.browser().chrome_version(),
        *reporting_metadata.FindStringByDottedPath("browser.chromeVersion"));

    if (is_cloud) {
      ASSERT_EQ(
          includes_device_info,
          !!reporting_metadata.FindStringByDottedPath("browser.machineUser"));
      if (metadata.browser().has_machine_user()) {
        ASSERT_EQ(
            metadata.browser().machine_user(),
            *reporting_metadata.FindStringByDottedPath("browser.machineUser"));
      }
    } else {
      ASSERT_TRUE(metadata.browser().has_machine_user());
    }
    // We check a field that should always be set if any device metadata was
    // provided.
    ASSERT_EQ(includes_device_info,
              metadata.has_device() && metadata.device().has_client_id());
    if (includes_device_info) {
      // The device DM token should only be populated when reporting is set at
      // the device level, aka not the profile level.
      if (profile_reporting) {
        ASSERT_FALSE(metadata.device().has_dm_token());
      } else {
        ASSERT_TRUE(metadata.device().has_dm_token());
        ASSERT_EQ(metadata.device().dm_token(), kFakeBrowserDMToken);
        ASSERT_TRUE(
            reporting_metadata.FindStringByDottedPath("device.dmToken"));
        ASSERT_EQ(metadata.device().dm_token(),
                  *reporting_metadata.FindStringByDottedPath("device.dmToken"));
      }

#if !BUILDFLAG(IS_CHROMEOS)
      ASSERT_TRUE(metadata.device().has_client_id());
      ASSERT_EQ(metadata.device().client_id(),
                *reporting_metadata.FindStringByDottedPath("device.clientId"));
#endif

      ASSERT_TRUE(metadata.device().has_os_version());
      ASSERT_EQ(metadata.device().os_version(),
                *reporting_metadata.FindStringByDottedPath("device.osVersion"));

      ASSERT_TRUE(metadata.device().has_os_platform());
      ASSERT_EQ(metadata.device().os_platform(), ExpectedOsPlatform());
      ASSERT_EQ(
          metadata.device().os_platform(),
          *reporting_metadata.FindStringByDottedPath("device.osPlatform"));

      ASSERT_TRUE(metadata.device().has_name());
      ASSERT_EQ(metadata.device().name(),
                *reporting_metadata.FindStringByDottedPath("device.name"));
    }

    ASSERT_TRUE(metadata.has_profile());

    ASSERT_TRUE(metadata.profile().has_dm_token());
    ASSERT_EQ(metadata.profile().dm_token(), kFakeProfileDMToken);
    ASSERT_EQ(metadata.profile().dm_token(),
              *reporting_metadata.FindStringByDottedPath("profile.dmToken"));

    ASSERT_TRUE(metadata.profile().has_gaia_email());
    ASSERT_EQ(metadata.profile().gaia_email(),
              *reporting_metadata.FindStringByDottedPath("profile.gaiaEmail"));

    ASSERT_TRUE(metadata.profile().has_profile_path());
    ASSERT_EQ(
        metadata.profile().profile_path(),
        *reporting_metadata.FindStringByDottedPath("profile.profilePath"));

    ASSERT_TRUE(metadata.profile().has_profile_name());
    ASSERT_EQ(
        metadata.profile().profile_name(),
        *reporting_metadata.FindStringByDottedPath("profile.profileName"));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    ASSERT_TRUE(metadata.profile().has_client_id());
    ASSERT_EQ(metadata.profile().client_id(), kFakeProfileClientId);
    ASSERT_EQ(metadata.profile().client_id(),
              *reporting_metadata.FindStringByDottedPath("profile.clientId"));
#endif
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ConnectorsServiceAnalysisProfileBrowserTest,
    testing::Combine(
        testing::Values(ManagementStatus::AFFILIATED,
                        ManagementStatus::UNAFFILIATED,
                        ManagementStatus::UNMANAGED),
        testing::Values(kNormalCloudAnalysisSettingsPref,
                        kNormalLocalAnalysisSettingsPref),
        testing::Bool()));

IN_PROC_BROWSER_TEST_P(ConnectorsServiceAnalysisProfileBrowserTest,
                       DeviceReporting) {
  SetPrefs(AnalysisConnectorPref(FILE_ATTACHED),
           AnalysisConnectorScopePref(FILE_ATTACHED), settings_value(),
           /*profile_scope*/ false);
  SetPrefs(kOnSecurityEventPref, kOnSecurityEventScopePref, settings_value(),
           /*profile_scope*/ false);
  auto settings =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetAnalysisSettings(GURL(kTestUrl), FILE_ATTACHED);

  // Expect no local analysis settings on platforms where it is unsupported.
#if !BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  if (settings_value() == kNormalLocalAnalysisSettingsPref) {
    ASSERT_FALSE(settings.has_value());
    return;
  }
#endif

  if (management_status() == ManagementStatus::UNMANAGED) {
    if (settings_value() == kNormalLocalAnalysisSettingsPref) {
      ASSERT_TRUE(settings.has_value());
      ASSERT_TRUE(settings.value().cloud_or_local_settings.is_local_analysis());
      ASSERT_EQ("path_user",
                settings.value().cloud_or_local_settings.local_path());
      ASSERT_TRUE(settings.value().cloud_or_local_settings.user_specific());
    } else {
      ASSERT_FALSE(settings.has_value());
    }
  } else {
    ASSERT_TRUE(settings.has_value());
    if (settings.value().cloud_or_local_settings.is_cloud_analysis()) {
      ASSERT_EQ(kFakeBrowserDMToken,
                settings.value().cloud_or_local_settings.dm_token());
      ValidateClientMetadata(*settings.value().client_metadata, is_cloud(),
                             /*profile_reporting*/ false);
    }
    ASSERT_FALSE(settings.value().per_profile);
  }

#if !BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ((management_status() == ManagementStatus::UNAFFILIATED) ? kDomain2
                                                                    : kDomain1,
            ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
                ->GetManagementDomain());
#endif
}

IN_PROC_BROWSER_TEST_P(ConnectorsServiceAnalysisProfileBrowserTest,
                       ProfileReporting) {
  SetPrefs(AnalysisConnectorPref(FILE_DOWNLOADED),
           AnalysisConnectorScopePref(FILE_DOWNLOADED), settings_value());
  SetPrefs(kOnSecurityEventPref, kOnSecurityEventScopePref,
           kNormalReportingSettingsPref);
  auto settings =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetAnalysisSettings(GURL(kTestUrl), FILE_DOWNLOADED);

  // Expect no local analysis settings on platforms where it is unsupported.
#if !BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  if (settings_value() == kNormalLocalAnalysisSettingsPref) {
    ASSERT_FALSE(settings.has_value());
    return;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  if (management_status() == ManagementStatus::UNMANAGED) {
    if (settings_value() == kNormalLocalAnalysisSettingsPref) {
      ASSERT_TRUE(settings.has_value());
      // TODO(b/238216275): Verify the metadata has the expected values.
    } else {
      ASSERT_FALSE(settings.has_value());
    }
  } else {
    ASSERT_TRUE(settings.has_value());
    if (settings.value().cloud_or_local_settings.is_cloud_analysis()) {
      ASSERT_EQ(kFakeBrowserDMToken,
                settings.value().cloud_or_local_settings.dm_token());
      ValidateClientMetadata(*settings.value().client_metadata, is_cloud(),
                             /*profile_reporting*/ false);
    }
    ASSERT_FALSE(settings.value().per_profile);
  }
#else
  std::string management_domain =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetManagementDomain();
  switch (management_status()) {
    case ManagementStatus::UNAFFILIATED:
      if (settings_value() == kNormalLocalAnalysisSettingsPref) {
        ASSERT_TRUE(settings.has_value());
        ASSERT_TRUE(
            settings.value().cloud_or_local_settings.is_local_analysis());
        ASSERT_EQ("path_user",
                  settings.value().cloud_or_local_settings.local_path());
        ASSERT_TRUE(settings.value().cloud_or_local_settings.user_specific());
        ASSERT_FALSE(management_domain.empty());
      } else {
        EXPECT_TRUE(settings.has_value());
        ASSERT_TRUE(
            settings.value().cloud_or_local_settings.is_cloud_analysis());
        ASSERT_EQ(kFakeProfileDMToken,
                  settings.value().cloud_or_local_settings.dm_token());
        ValidateClientMetadata(*settings.value().client_metadata, is_cloud(),
                               /*profile_reporting*/ true);
        ASSERT_TRUE(settings.value().per_profile);
        ASSERT_EQ(kDomain1, management_domain);
      }
      break;
    case ManagementStatus::AFFILIATED:
      EXPECT_TRUE(settings.has_value());
      if (settings.value().cloud_or_local_settings.is_cloud_analysis()) {
        ASSERT_EQ(kFakeProfileDMToken,
                  settings.value().cloud_or_local_settings.dm_token());
        ValidateClientMetadata(*settings.value().client_metadata, is_cloud(),
                               /*profile_reporting*/ true);
      }
      ASSERT_TRUE(settings.value().per_profile);
      ASSERT_EQ(kDomain1, management_domain);
      break;
    case ManagementStatus::UNMANAGED:
      EXPECT_TRUE(settings.has_value());
      ASSERT_TRUE(settings.value().client_metadata);
      if (settings.value().cloud_or_local_settings.is_cloud_analysis()) {
        ASSERT_EQ(kFakeProfileDMToken,
                  settings.value().cloud_or_local_settings.dm_token());
        ValidateClientMetadata(*settings.value().client_metadata, is_cloud(),
                               /*profile_reporting*/ true);
        ASSERT_TRUE(settings.value().per_profile);
      } else {
        EXPECT_EQ("path_user",
                  settings.value().cloud_or_local_settings.local_path());
        EXPECT_TRUE(settings.value().cloud_or_local_settings.user_specific());
        ValidateClientMetadata(*settings.value().client_metadata, is_cloud(),
                               /*profile_reporting*/ true);
      }
      ASSERT_EQ(kDomain1, management_domain);
      break;
  }
#endif
}

IN_PROC_BROWSER_TEST_P(ConnectorsServiceAnalysisProfileBrowserTest,
                       NoReporting) {
  SetPrefs(AnalysisConnectorPref(PRINT), AnalysisConnectorScopePref(PRINT),
           settings_value());
  auto settings =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetAnalysisSettings(GURL(kTestUrl), PRINT);

  // Expect no local analysis settings on platforms where it is unsupported.
#if !BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  if (settings_value() == kNormalLocalAnalysisSettingsPref) {
    ASSERT_FALSE(settings.has_value());
    return;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  if (management_status() == ManagementStatus::UNMANAGED) {
    ASSERT_FALSE(settings.has_value());
  } else {
    ASSERT_TRUE(settings.has_value());
    ASSERT_TRUE(settings.value().cloud_or_local_settings.is_cloud_analysis());
    ASSERT_EQ(kFakeBrowserDMToken,
              settings.value().cloud_or_local_settings.dm_token());
    if (enterprise_connectors_enabled_on_mgs()) {
      ASSERT_FALSE(ContainsClientId(settings.value()));
    }
    ASSERT_TRUE(settings.value().client_metadata);
    ASSERT_FALSE(settings.value().per_profile);
  }
#else
  std::string management_domain =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetManagementDomain();
  switch (management_status()) {
    case ManagementStatus::UNAFFILIATED:
      if (settings_value() == kNormalLocalAnalysisSettingsPref) {
        ASSERT_TRUE(settings.has_value());
        ASSERT_TRUE(
            settings.value().cloud_or_local_settings.is_local_analysis());
        ASSERT_EQ("path_user",
                  settings.value().cloud_or_local_settings.local_path());
        ASSERT_TRUE(settings.value().cloud_or_local_settings.user_specific());
        ASSERT_NE(management_domain.empty(), true);
      } else {
        EXPECT_TRUE(settings.has_value());
        ASSERT_EQ(kFakeProfileDMToken,
                  settings.value().cloud_or_local_settings.dm_token());
        if (enterprise_connectors_enabled_on_mgs()) {
          ASSERT_FALSE(ContainsClientId(settings.value()));
        }
        ASSERT_TRUE(settings.value().client_metadata);
        ASSERT_EQ(management_domain, kDomain1);
      }
      break;
    case ManagementStatus::AFFILIATED:
      EXPECT_TRUE(settings.has_value());
      if (settings.value().cloud_or_local_settings.is_cloud_analysis()) {
        ASSERT_EQ(kFakeProfileDMToken,
                  settings.value().cloud_or_local_settings.dm_token());
        if (enterprise_connectors_enabled_on_mgs()) {
          ASSERT_FALSE(ContainsClientId(settings.value()));
        }
        ASSERT_TRUE(settings.value().client_metadata);
        ASSERT_FALSE(
            settings.value().client_metadata->profile().dm_token().empty());
        ASSERT_FALSE(
            settings.value().client_metadata->device().dm_token().empty());
      } else {
        ASSERT_EQ("path_user",
                  settings.value().cloud_or_local_settings.local_path());
        ASSERT_TRUE(settings.value().cloud_or_local_settings.user_specific());
        ASSERT_TRUE(settings.value().client_metadata);
      }
      ASSERT_TRUE(settings.value().per_profile);
      ASSERT_EQ(kDomain1, management_domain);
      break;
    case ManagementStatus::UNMANAGED:
      EXPECT_TRUE(settings.has_value());
      if (settings.value().cloud_or_local_settings.is_cloud_analysis()) {
        ASSERT_EQ(kFakeProfileDMToken,
                  settings.value().cloud_or_local_settings.dm_token());
        if (enterprise_connectors_enabled_on_mgs()) {
          ASSERT_FALSE(ContainsClientId(settings.value()));
        }
        ASSERT_TRUE(settings.value().client_metadata);
        ASSERT_FALSE(
            settings.value().client_metadata->profile().dm_token().empty());
        ASSERT_TRUE(
            settings.value().client_metadata->device().dm_token().empty());

      } else {
        ASSERT_EQ("path_user",
                  settings.value().cloud_or_local_settings.local_path());
        ASSERT_TRUE(settings.value().cloud_or_local_settings.user_specific());
        ASSERT_TRUE(settings.value().client_metadata);
      }
      ASSERT_TRUE(settings.value().per_profile);
      ASSERT_EQ(kDomain1, management_domain);
      break;
  }
#endif
}

// TODO(b/302576851): Consider removing after EnableRelaxedAffiliationCheck
// is cleaned up.
IN_PROC_BROWSER_TEST_P(ConnectorsServiceAnalysisProfileBrowserTest,
                       Affiliation) {
  SetPrefs(AnalysisConnectorPref(BULK_DATA_ENTRY),
           AnalysisConnectorScopePref(BULK_DATA_ENTRY), settings_value());
  auto settings =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetAnalysisSettings(GURL(kTestUrl), BULK_DATA_ENTRY);

  if (settings_value() == kNormalLocalAnalysisSettingsPref) {
    // Expect no local analysis settings on platforms where it is unsupported.
#if !BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
    ASSERT_FALSE(settings.has_value());
#else
    // Since they don't use anything tied to DM tokens and rely on a separately
    // installed agent, local analysis policies are always returned regardless
    // of the management status.
    ASSERT_TRUE(settings.has_value());
    ASSERT_TRUE(settings.value().cloud_or_local_settings.is_local_analysis());
    ASSERT_TRUE(settings.value().per_profile);
    ASSERT_EQ("path_user",
              settings.value().cloud_or_local_settings.local_path());
    ASSERT_TRUE(settings.value().cloud_or_local_settings.user_specific());
#endif
  } else {
#if BUILDFLAG(IS_CHROMEOS)
    if (management_status() == ManagementStatus::UNMANAGED) {
      ASSERT_FALSE(settings.has_value());
    } else {
      ASSERT_TRUE(settings.has_value());
      ASSERT_TRUE(settings.value().cloud_or_local_settings.is_cloud_analysis());
      ASSERT_EQ(kFakeBrowserDMToken,
                settings.value().cloud_or_local_settings.dm_token());
      ASSERT_FALSE(settings.value().per_profile);
    }
#else
    switch (management_status()) {
      case ManagementStatus::UNAFFILIATED:
      case ManagementStatus::AFFILIATED:
        EXPECT_TRUE(settings.has_value());
        ASSERT_TRUE(
            settings.value().cloud_or_local_settings.is_cloud_analysis());
        ASSERT_EQ(kFakeProfileDMToken,
                  settings.value().cloud_or_local_settings.dm_token());
        if (enterprise_connectors_enabled_on_mgs()) {
          ASSERT_FALSE(ContainsClientId(settings.value()));
        }
        ASSERT_TRUE(settings.value().client_metadata);
        ASSERT_TRUE(settings.value().per_profile);
        break;
      case ManagementStatus::UNMANAGED:
        EXPECT_TRUE(settings.has_value());
        ASSERT_EQ(kFakeProfileDMToken,
                  settings.value().cloud_or_local_settings.dm_token());
        if (enterprise_connectors_enabled_on_mgs()) {
          ASSERT_FALSE(ContainsClientId(settings.value()));
        }
        ASSERT_TRUE(settings.value().client_metadata);
        ASSERT_TRUE(settings.value().per_profile);
        break;
    }
#endif
  }
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
  SetPrefs(kEnterpriseRealTimeUrlCheckMode, kEnterpriseRealTimeUrlCheckScope,
           1);
  auto maybe_dm_token =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetDMTokenForRealTimeUrlCheck();
  EnterpriseRealTimeUrlCheckMode url_check_pref =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetAppliedRealTimeUrlCheck();

#if BUILDFLAG(IS_CHROMEOS)
  if (management_status() == ManagementStatus::UNMANAGED) {
    ASSERT_FALSE(maybe_dm_token.has_value());
    ASSERT_EQ(REAL_TIME_CHECK_DISABLED, url_check_pref);
  } else {
    ASSERT_TRUE(maybe_dm_token.has_value());
    ASSERT_EQ(kFakeBrowserDMToken, maybe_dm_token.value());
    ASSERT_EQ(REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED, url_check_pref);
  }
#else
  std::string management_domain =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetManagementDomain();
  // expected state is the same regardless of management_status() value.
  ASSERT_TRUE(maybe_dm_token.has_value());
  ASSERT_EQ(kFakeProfileDMToken, maybe_dm_token.value());
  ASSERT_EQ(REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED, url_check_pref);
  ASSERT_EQ(kDomain1, management_domain);
#endif
}

}  // namespace enterprise_connectors
