// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string_view>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/enterprise_reporting_private.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "net/base/features.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/dm_token_utils.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/device_signals/core/common/signals_features.h"
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

constexpr char kAndAnotherServiceProvider[] = R"({
      "service_provider": "and_another",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }
      ]
    })";

constexpr char kRequestingUrl[] = "https://www.example.com";

class MockClientCertStore : public net::ClientCertStore {
 public:
  explicit MockClientCertStore(net::ClientCertIdentityList certs)
      : certs_(std::move(certs)) {}

  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    std::move(callback).Run(std::move(certs_));
  }

  net::ClientCertIdentityList certs_;
};

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
#if BUILDFLAG(IS_CHROMEOS)
    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
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
    feature_list_.InitAndEnableFeature(net::features::kAsyncDns);
  }

  bool browser_managed() const { return testing::get<0>(GetParam()); }

  bool profile_managed() const { return testing::get<1>(GetParam()); }

  void SetUpOnMainThread() override {
    EnterpriseReportingPrivateGetContextInfoBaseBrowserTest::
        SetUpOnMainThread();

    if (browser_managed()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      auto* browser_policy_manager = g_browser_process->platform_part()
                                         ->browser_policy_connector_ash()
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
          browser()->profile()->GetUserCloudPolicyManagerAsh();
      profile_policy_manager->core()->client()->SetupRegistration(
          "dm_token", "client_id", {});
#else
      enterprise_connectors::test::SetProfileDMToken(browser()->profile(),
                                                     "dm_token");
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

class EnterpriseReportingPrivateGetContextInfoSiteIsolationTest
    : public EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  bool site_isolation_enabled() { return GetParam(); }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (site_isolation_enabled()) {
      command_line->AppendSwitch(switches::kSitePerProcess);
    } else {
      command_line->RemoveSwitch(switches::kSitePerProcess);
      command_line->AppendSwitch(switches::kDisableSiteIsolation);
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoSiteIsolationTest,
    testing::Bool());

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseReportingPrivateGetContextInfoBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(
    EnterpriseReportingPrivateGetContextInfoSiteIsolationTest,
    Test) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = api_test_utils::RunFunctionAndReturnSingleResult(
      function.get(),
      /*args*/ "[]", browser()->profile());
  ASSERT_TRUE(context_info_value);
  ASSERT_TRUE(context_info_value->is_dict());

  auto info = enterprise_reporting_private::ContextInfo::FromValue(
      context_info_value->GetDict());
  ASSERT_TRUE(info);

  EXPECT_TRUE(info->browser_affiliation_ids.empty());
  EXPECT_TRUE(info->profile_affiliation_ids.empty());
  EXPECT_TRUE(info->on_file_attached_providers.empty());
  EXPECT_TRUE(info->on_file_downloaded_providers.empty());
  EXPECT_TRUE(info->on_bulk_data_entry_providers.empty());
  EXPECT_TRUE(info->on_print_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled,
            info->realtime_url_check_mode);
  EXPECT_TRUE(info->on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info->browser_version);
  EXPECT_EQ(site_isolation_enabled(), info->site_isolation_enabled);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class EnterpriseReportingPrivateGetContextInfoChromeOSFirewallTest
    : public EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  bool dev_mode_enabled() { return GetParam(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (dev_mode_enabled()) {
      command_line->AppendSwitch(chromeos::switches::kSystemDevMode);
    } else {
      command_line->RemoveSwitch(chromeos::switches::kSystemDevMode);
    }
  }

  bool BuiltInDnsClientPlatformDefault() {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    return true;
#else
    return false;
#endif
  }

  void ExpectDefaultThirdPartyBlockingEnabled(
      const enterprise_reporting_private::ContextInfo& info) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    EXPECT_TRUE(*info.third_party_blocking_enabled);
#else
    EXPECT_FALSE(info.third_party_blocking_enabled.has_value());
#endif
  }
};

IN_PROC_BROWSER_TEST_P(
    EnterpriseReportingPrivateGetContextInfoChromeOSFirewallTest,
    Test) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = api_test_utils::RunFunctionAndReturnSingleResult(
      function.get(),
      /*args*/ "[]", browser()->profile());
  ASSERT_TRUE(context_info_value);
  ASSERT_TRUE(context_info_value->is_dict());

  auto info = enterprise_reporting_private::ContextInfo::FromValue(
      context_info_value->GetDict());
  ASSERT_TRUE(info);

  EXPECT_TRUE(info->browser_affiliation_ids.empty());
  EXPECT_TRUE(info->profile_affiliation_ids.empty());
  EXPECT_TRUE(info->on_file_attached_providers.empty());
  EXPECT_TRUE(info->on_file_downloaded_providers.empty());
  EXPECT_TRUE(info->on_bulk_data_entry_providers.empty());
  EXPECT_TRUE(info->on_print_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled,
            info->realtime_url_check_mode);
  EXPECT_TRUE(info->on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info->browser_version);
  EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kStandard,
            info->safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info->built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PasswordProtectionTrigger::kPolicyUnset,
      info->password_protection_warning_trigger);
  EXPECT_FALSE(info->chrome_remote_desktop_app_blocked);
  ExpectDefaultThirdPartyBlockingEnabled(*info);
  EXPECT_EQ(dev_mode_enabled()
                ? api::enterprise_reporting_private::SettingValue::kUnknown
                : api::enterprise_reporting_private::SettingValue::kEnabled,
            info->os_firewall);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoChromeOSFirewallTest,
    testing::Bool());
#endif

// crbug.com/1230268 not working on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_AffiliationIDs DISABLED_AffiliationIDs
#else
#define MAYBE_AffiliationIDs AffiliationIDs
#endif
IN_PROC_BROWSER_TEST_P(EnterpriseReportingPrivateGetContextInfoBrowserTest,
                       MAYBE_AffiliationIDs) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = api_test_utils::RunFunctionAndReturnSingleResult(
      function.get(),
      /*args*/ "[]", browser()->profile());
  ASSERT_TRUE(context_info_value);
  ASSERT_TRUE(context_info_value->is_dict());

  auto info = enterprise_reporting_private::ContextInfo::FromValue(
      context_info_value->GetDict());
  ASSERT_TRUE(info);

  if (profile_managed()) {
    EXPECT_EQ(2u, info->profile_affiliation_ids.size());
    EXPECT_EQ(kProfileID1, info->profile_affiliation_ids[0]);
    EXPECT_EQ(kProfileID2, info->profile_affiliation_ids[1]);
  } else {
    EXPECT_TRUE(info->profile_affiliation_ids.empty());
  }

  if (browser_managed()) {
    EXPECT_EQ(2u, info->browser_affiliation_ids.size());
    EXPECT_EQ(kBrowserID1, info->browser_affiliation_ids[0]);
    EXPECT_EQ(kBrowserID2, info->browser_affiliation_ids[1]);
  } else {
    EXPECT_TRUE(info->browser_affiliation_ids.empty());
  }

  EXPECT_TRUE(info->on_file_attached_providers.empty());
  EXPECT_TRUE(info->on_file_downloaded_providers.empty());
  EXPECT_TRUE(info->on_bulk_data_entry_providers.empty());
  EXPECT_TRUE(info->on_print_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled,
            info->realtime_url_check_mode);
  EXPECT_TRUE(info->on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info->browser_version);
  EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kStandard,
            info->safe_browsing_protection_level);
  EXPECT_TRUE(info->built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PasswordProtectionTrigger::kPolicyUnset,
      info->password_protection_warning_trigger);
  EXPECT_FALSE(info->chrome_remote_desktop_app_blocked);
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(*info->third_party_blocking_enabled);
#else
  EXPECT_FALSE(info->third_party_blocking_enabled.has_value());
#endif
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestFileAttachedProviderName) {
  SetupDMToken();
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), enterprise_connectors::FILE_ATTACHED,
      kGoogleServiceProvider);

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = api_test_utils::RunFunctionAndReturnSingleResult(
      function.get(),
      /*args*/ "[]", browser()->profile());
  ASSERT_TRUE(context_info_value);
  ASSERT_TRUE(context_info_value->is_dict());

  auto info = enterprise_reporting_private::ContextInfo::FromValue(
      context_info_value->GetDict());
  ASSERT_TRUE(info);

  EXPECT_EQ(0UL, info->on_file_downloaded_providers.size());
  EXPECT_EQ(0UL, info->on_bulk_data_entry_providers.size());
  EXPECT_EQ(0UL, info->on_print_providers.size());

  EXPECT_EQ(1UL, info->on_file_attached_providers.size());
  EXPECT_EQ("google", info->on_file_attached_providers[0]);
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestFileDownloadedProviderName) {
  SetupDMToken();
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED,
      kGoogleServiceProvider);

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = api_test_utils::RunFunctionAndReturnSingleResult(
      function.get(),
      /*args*/ "[]", browser()->profile());
  ASSERT_TRUE(context_info_value);
  ASSERT_TRUE(context_info_value->is_dict());

  auto info = enterprise_reporting_private::ContextInfo::FromValue(
      context_info_value->GetDict());
  ASSERT_TRUE(info);

  EXPECT_EQ(0UL, info->on_file_attached_providers.size());
  EXPECT_EQ(0UL, info->on_bulk_data_entry_providers.size());
  EXPECT_EQ(0UL, info->on_print_providers.size());

  EXPECT_EQ(1UL, info->on_file_downloaded_providers.size());
  EXPECT_EQ("google", info->on_file_downloaded_providers[0]);
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestBulkDataEntryProviderName) {
  SetupDMToken();
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
      kGoogleServiceProvider);

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = api_test_utils::RunFunctionAndReturnSingleResult(
      function.get(),
      /*args*/ "[]", browser()->profile());
  ASSERT_TRUE(context_info_value);
  ASSERT_TRUE(context_info_value->is_dict());

  auto info = enterprise_reporting_private::ContextInfo::FromValue(
      context_info_value->GetDict());
  ASSERT_TRUE(info);

  EXPECT_EQ(0UL, info->on_file_downloaded_providers.size());
  EXPECT_EQ(0UL, info->on_file_attached_providers.size());
  EXPECT_EQ(0UL, info->on_print_providers.size());

  EXPECT_EQ(1UL, info->on_bulk_data_entry_providers.size());
  EXPECT_EQ("google", info->on_bulk_data_entry_providers[0]);
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestPrintProviderName) {
  SetupDMToken();
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), enterprise_connectors::PRINT,
      kGoogleServiceProvider);

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = api_test_utils::RunFunctionAndReturnSingleResult(
      function.get(),
      /*args*/ "[]", browser()->profile());
  ASSERT_TRUE(context_info_value);
  ASSERT_TRUE(context_info_value->is_dict());

  auto info = enterprise_reporting_private::ContextInfo::FromValue(
      context_info_value->GetDict());
  ASSERT_TRUE(info);

  EXPECT_EQ(0UL, info->on_file_downloaded_providers.size());
  EXPECT_EQ(0UL, info->on_file_attached_providers.size());
  EXPECT_EQ(0UL, info->on_bulk_data_entry_providers.size());

  EXPECT_EQ(1UL, info->on_print_providers.size());
  EXPECT_EQ("google", info->on_print_providers[0]);
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestAllProviderNamesSet) {
  SetupDMToken();
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
      kGoogleServiceProvider);
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), enterprise_connectors::FILE_ATTACHED,
      kOtherServiceProvider);
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED,
      kAnotherServiceProvider);
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), enterprise_connectors::PRINT,
      kAndAnotherServiceProvider);

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = api_test_utils::RunFunctionAndReturnSingleResult(
      function.get(),
      /*args*/ "[]", browser()->profile());
  ASSERT_TRUE(context_info_value);
  ASSERT_TRUE(context_info_value->is_dict());

  auto info = enterprise_reporting_private::ContextInfo::FromValue(
      context_info_value->GetDict());
  ASSERT_TRUE(info);

  EXPECT_EQ(1UL, info->on_bulk_data_entry_providers.size());
  EXPECT_EQ("google", info->on_bulk_data_entry_providers[0]);

  EXPECT_EQ(1UL, info->on_file_attached_providers.size());
  EXPECT_EQ("other", info->on_file_attached_providers[0]);

  EXPECT_EQ(1UL, info->on_file_downloaded_providers.size());
  EXPECT_EQ("another", info->on_file_downloaded_providers[0]);

  EXPECT_EQ(1UL, info->on_print_providers.size());
  EXPECT_EQ("and_another", info->on_print_providers[0]);
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestOnSecurityEventProviderNameUnset) {
  SetupDMToken();

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = api_test_utils::RunFunctionAndReturnSingleResult(
      function.get(),
      /*args*/ "[]", browser()->profile());
  ASSERT_TRUE(context_info_value);
  ASSERT_TRUE(context_info_value->is_dict());

  auto info = enterprise_reporting_private::ContextInfo::FromValue(
      context_info_value->GetDict());
  ASSERT_TRUE(info);

  EXPECT_EQ(0UL, info->on_security_event_providers.size());
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateGetContextInfoBaseBrowserTest,
                       TestOnSecurityEventProviderNameSet) {
  SetupDMToken();
  enterprise_connectors::test::SetOnSecurityEventReporting(
      browser()->profile()->GetPrefs(),
      /* enabled= */ true);

  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetContextInfoFunction>();
  auto context_info_value = api_test_utils::RunFunctionAndReturnSingleResult(
      function.get(),
      /*args*/ "[]", browser()->profile());
  ASSERT_TRUE(context_info_value);
  ASSERT_TRUE(context_info_value->is_dict());

  auto info = enterprise_reporting_private::ContextInfo::FromValue(
      context_info_value->GetDict());
  ASSERT_TRUE(info);

  EXPECT_EQ(1UL, info->on_security_event_providers.size());
  // test::SetOnSecurityEventReporting sets the provider name to google
  EXPECT_EQ("google", info->on_security_event_providers[0]);
}

class EnterpriseReportingPrivateGetCertificateTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  EnterpriseReportingPrivateGetCertificateTest() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    feature_list_.InitWithFeatureState(
        enterprise_signals::features::kAllowClientCertificateReportingForUsers,
        GetParam());
#endif
  }

  void SetUpOnMainThread() override {
    ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
        ->set_client_cert_store_factory_for_testing(base::BindRepeating(
            &EnterpriseReportingPrivateGetCertificateTest::CreateCertStore,
            base::Unretained(this)));
  }

  void SetPolicyValue(const std::string& policy_value) {
    EXPECT_FALSE(enterprise_util::IsMachinePolicyPref(
        prefs::kManagedAutoSelectCertificateForUrls));

    base::Value::List list;
    list.Append(policy_value);

    policy::PolicyMap policies;
    policies.Set(policy::key::kAutoSelectCertificateForUrls,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(std::move(list)),
                 nullptr);
    UpdateProviderPolicy(policies);

    EXPECT_TRUE(enterprise_util::IsMachinePolicyPref(
        prefs::kManagedAutoSelectCertificateForUrls));
  }

  void SetUserPolicyValue(const std::string& policy_value) {
    EXPECT_FALSE(enterprise_util::IsMachinePolicyPref(
        prefs::kManagedAutoSelectCertificateForUrls));
    base::Value::List list;
    list.Append(policy_value);

    policy::PolicyMap policies;
    policies.Set(policy::key::kAutoSelectCertificateForUrls,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(std::move(list)),
                 nullptr);
    UpdateProviderPolicy(policies);
  }

  enterprise_reporting_private::Certificate GetCertificate() {
    auto function = base::MakeRefCounted<
        EnterpriseReportingPrivateGetCertificateFunction>();

    std::string params = "[\"";
    params += kRequestingUrl;
    params += "\"]";

    auto certificate_value = api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), params, browser()->profile());
    EXPECT_TRUE(certificate_value);
    EXPECT_TRUE(certificate_value->is_dict());

    auto cert = enterprise_reporting_private::Certificate::FromValue(
        certificate_value->GetDict());
    EXPECT_TRUE(cert);

    return std::move(cert).value_or(
        enterprise_reporting_private::Certificate());
  }

  void SetupDefaultClientCertList() {
    EXPECT_EQ(0UL, client_certs_.size());

    client_certs_.push_back(
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem"));
    client_certs_.push_back(
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_2.pem"));
  }

  std::vector<scoped_refptr<net::X509Certificate>>& client_certs() {
    return client_certs_;
  }

 private:
  std::unique_ptr<net::ClientCertStore> CreateCertStore() {
    return std::make_unique<MockClientCertStore>(
        net::FakeClientCertIdentityListFromCertificateList(client_certs_));
  }

  std::vector<scoped_refptr<net::X509Certificate>> client_certs_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(EnterpriseReportingPrivateGetCertificateTest,
                       TestPolicyUnset) {
  auto cert = GetCertificate();

  EXPECT_EQ(enterprise_reporting_private::CertificateStatus::kPolicyUnset,
            cert.status);
}

IN_PROC_BROWSER_TEST_P(EnterpriseReportingPrivateGetCertificateTest,
                       TestPolicySet) {
  constexpr char kPolicyValue[] = R"({
      "pattern": "https://www.example.com",
      "filter": {
        "ISSUER": {
          "CN": "B CA"
        }
      }
    })";
  SetPolicyValue(kPolicyValue);

  auto cert = GetCertificate();

  EXPECT_EQ(enterprise_reporting_private::CertificateStatus::kOk, cert.status);
  EXPECT_FALSE(cert.encoded_certificate.has_value());
}

IN_PROC_BROWSER_TEST_P(EnterpriseReportingPrivateGetCertificateTest,
                       TestPolicySetCertsPresentButNotMatching) {
  SetupDefaultClientCertList();

  constexpr char kPolicyValue[] = R"({
      "pattern": "https://www.example.com",
      "filter": {
        "ISSUER": {
          "CN": "BAD CA"
        }
      }
    })";
  SetPolicyValue(kPolicyValue);

  auto cert = GetCertificate();

  EXPECT_EQ(enterprise_reporting_private::CertificateStatus::kOk, cert.status);
  EXPECT_FALSE(cert.encoded_certificate.has_value());
}

IN_PROC_BROWSER_TEST_P(EnterpriseReportingPrivateGetCertificateTest,
                       TestPolicySetCertsPresentUrlNotMatching) {
  SetupDefaultClientCertList();

  constexpr char kPolicyValue[] = R"({
      "pattern": "https://www.bad.example.com",
      "filter": {
        "ISSUER": {
          "CN": "B CA"
        }
      }
    })";
  SetPolicyValue(kPolicyValue);

  auto cert = GetCertificate();

  EXPECT_EQ(enterprise_reporting_private::CertificateStatus::kOk, cert.status);
  EXPECT_FALSE(cert.encoded_certificate.has_value());
}

IN_PROC_BROWSER_TEST_P(EnterpriseReportingPrivateGetCertificateTest,
                       TestPolicySetCertsPresentAndMatching) {
  SetupDefaultClientCertList();

  constexpr char kPolicyValue[] = R"({
      "pattern": "https://www.example.com",
      "filter": {
        "ISSUER": {
          "CN": "B CA"
        }
      }
    })";
  SetPolicyValue(kPolicyValue);

  auto cert = GetCertificate();

  EXPECT_EQ(enterprise_reporting_private::CertificateStatus::kOk, cert.status);
  EXPECT_TRUE(cert.encoded_certificate.has_value());

  std::string_view der_cert = net::x509_util::CryptoBufferAsStringPiece(
      client_certs()[0]->cert_buffer());
  std::vector<uint8_t> expected_der_bytes(der_cert.begin(), der_cert.end());

  EXPECT_EQ(expected_der_bytes, *cert.encoded_certificate);
}

IN_PROC_BROWSER_TEST_P(EnterpriseReportingPrivateGetCertificateTest,
                       TestUserPolicySetCertsPresentAndMatching) {
  SetupDefaultClientCertList();

  constexpr char kPolicyValue[] = R"({
      "pattern": "https://www.example.com",
      "filter": {
        "ISSUER": {
          "CN": "B CA"
        }
      }
    })";
  SetUserPolicyValue(kPolicyValue);

  auto cert = GetCertificate();

  EXPECT_EQ(enterprise_reporting_private::CertificateStatus::kOk, cert.status);
  EXPECT_TRUE(cert.encoded_certificate.has_value());

  std::string_view der_cert = net::x509_util::CryptoBufferAsStringPiece(
      client_certs()[0]->cert_buffer());
  std::vector<uint8_t> expected_der_bytes(der_cert.begin(), der_cert.end());

  EXPECT_EQ(expected_der_bytes, *cert.encoded_certificate);
}

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseReportingPrivateGetCertificateTest,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
                         testing::Bool());
#else
                         testing::Values(false));
#endif

}  // namespace extensions
