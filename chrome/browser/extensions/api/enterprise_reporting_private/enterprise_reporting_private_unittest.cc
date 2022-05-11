// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/files/file_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/signals/signals_common.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/extensions/api/enterprise_reporting_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/component_updater/pref_names.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/missive/missive_client.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <netfw.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/test/test_reg_util_win.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/nix/xdg_util.h"
#endif

namespace enterprise_reporting_private =
    ::extensions::api::enterprise_reporting_private;

using SettingValue = enterprise_signals::SettingValue;

namespace extensions {

#if !BUILDFLAG(IS_CHROMEOS)

namespace {

const char kFakeClientId[] = "fake-client-id";

}  // namespace

// Test for API enterprise.reportingPrivate.getDeviceId
class EnterpriseReportingPrivateGetDeviceIdTest : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateGetDeviceIdTest() = default;

  EnterpriseReportingPrivateGetDeviceIdTest(
      const EnterpriseReportingPrivateGetDeviceIdTest&) = delete;
  EnterpriseReportingPrivateGetDeviceIdTest& operator=(
      const EnterpriseReportingPrivateGetDeviceIdTest&) = delete;

  void SetClientId(const std::string& client_id) {
    storage_.SetClientId(client_id);
  }

 private:
  policy::FakeBrowserDMTokenStorage storage_;
};

TEST_F(EnterpriseReportingPrivateGetDeviceIdTest, GetDeviceId) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceIdFunction>();
  SetClientId(kFakeClientId);
  std::unique_ptr<base::Value> id =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(id);
  ASSERT_TRUE(id->is_string());
  EXPECT_EQ(kFakeClientId, id->GetString());
}

TEST_F(EnterpriseReportingPrivateGetDeviceIdTest, DeviceIdNotExist) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceIdFunction>();
  SetClientId("");
  ASSERT_EQ(enterprise_reporting::kDeviceIdNotFound,
            RunFunctionAndReturnError(function.get(), "[]"));
}

// Test for API enterprise.reportingPrivate.getDeviceId
class EnterpriseReportingPrivateDeviceDataFunctionsTest
    : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateDeviceDataFunctionsTest() = default;

  EnterpriseReportingPrivateDeviceDataFunctionsTest(
      const EnterpriseReportingPrivateDeviceDataFunctionsTest&) = delete;
  EnterpriseReportingPrivateDeviceDataFunctionsTest& operator=(
      const EnterpriseReportingPrivateDeviceDataFunctionsTest&) = delete;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    ASSERT_TRUE(fake_appdata_dir_.CreateUniqueTempDir());
    OverrideEndpointVerificationDirForTesting(fake_appdata_dir_.GetPath());
  }

 private:
  base::ScopedTempDir fake_appdata_dir_;
};

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, StoreDeviceData) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values = std::make_unique<base::ListValue>();
  values->Append("a");
  values->Append(base::Value(base::Value::BlobStorage({1, 2, 3})));
  extension_function_test_utils::RunFunction(function.get(), std::move(values),
                                             browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(function->GetResultList());
  EXPECT_EQ(0u, function->GetResultList()->size());
  EXPECT_TRUE(function->GetError().empty());
}

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, DeviceDataMissing) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values = std::make_unique<base::ListValue>();
  values->Append("b");
  extension_function_test_utils::RunFunction(function.get(), std::move(values),
                                             browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(function->GetResultList());
  EXPECT_EQ(1u, function->GetResultList()->size());
  EXPECT_TRUE(function->GetError().empty());

  const base::Value& single_result = (*function->GetResultList())[0];
  ASSERT_TRUE(single_result.is_blob());
  EXPECT_EQ(base::Value::BlobStorage(), single_result.GetBlob());
}

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, DeviceBadId) {
  auto set_function =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> set_values =
      std::make_unique<base::ListValue>();
  set_values->Append("a/b");
  set_values->Append(base::Value(base::Value::BlobStorage({1, 2, 3})));
  extension_function_test_utils::RunFunction(set_function.get(),
                                             std::move(set_values), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(set_function->GetError().empty());

  // Try to read the directory as a file and should fail.
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values = std::make_unique<base::ListValue>();
  values->Append("a");
  extension_function_test_utils::RunFunction(function.get(), std::move(values),
                                             browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(function->GetResultList());
  EXPECT_EQ(0u, function->GetResultList()->size());
  EXPECT_FALSE(function->GetError().empty());
}

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, RetrieveDeviceData) {
  auto set_function =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> set_values =
      std::make_unique<base::ListValue>();
  set_values->Append("c");
  set_values->Append(base::Value(base::Value::BlobStorage({1, 2, 3})));
  extension_function_test_utils::RunFunction(set_function.get(),
                                             std::move(set_values), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(set_function->GetError().empty());

  auto get_function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values = std::make_unique<base::ListValue>();
  values->Append("c");
  extension_function_test_utils::RunFunction(get_function.get(),
                                             std::move(values), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(get_function->GetResultList());
  const base::Value& single_result = (*get_function->GetResultList())[0];
  EXPECT_TRUE(get_function->GetError().empty());
  ASSERT_TRUE(single_result.is_blob());
  EXPECT_EQ(base::Value::BlobStorage({1, 2, 3}), single_result.GetBlob());

  // Clear the data and check that it is gone.
  auto set_function2 =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> reset_values =
      std::make_unique<base::ListValue>();
  reset_values->Append("c");
  extension_function_test_utils::RunFunction(set_function2.get(),
                                             std::move(reset_values), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(set_function2->GetError().empty());

  auto get_function2 =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values2 =
      std::make_unique<base::ListValue>();
  values2->Append("c");
  extension_function_test_utils::RunFunction(get_function2.get(),
                                             std::move(values2), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(get_function2->GetResultList());
  EXPECT_EQ(1u, get_function2->GetResultList()->size());
  EXPECT_TRUE(get_function2->GetError().empty());

  const base::Value& single_result2 = (*get_function2->GetResultList())[0];
  ASSERT_TRUE(single_result2.is_blob());
  EXPECT_EQ(base::Value::BlobStorage(), single_result2.GetBlob());
}

// TODO(pastarmovj): Remove once implementation for the other platform exists.
#if BUILDFLAG(IS_WIN)

// Test for API enterprise.reportingPrivate.getDeviceId
class EnterpriseReportingPrivateGetPersistentSecretFunctionTest
    : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateGetPersistentSecretFunctionTest() = default;

  EnterpriseReportingPrivateGetPersistentSecretFunctionTest(
      const EnterpriseReportingPrivateGetPersistentSecretFunctionTest&) =
      delete;
  EnterpriseReportingPrivateGetPersistentSecretFunctionTest& operator=(
      const EnterpriseReportingPrivateGetPersistentSecretFunctionTest&) =
      delete;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
#if BUILDFLAG(IS_WIN)
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
#endif
  }

 private:
#if BUILDFLAG(IS_WIN)
  registry_util::RegistryOverrideManager registry_override_manager_;
#endif
};

TEST_F(EnterpriseReportingPrivateGetPersistentSecretFunctionTest, GetSecret) {
  auto function = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::unique_ptr<base::Value> result1 =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(result1);
  ASSERT_TRUE(result1->is_blob());
  auto generated_blob = result1->GetBlob();

  // Re-running should not change the secret.
  auto function2 = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::unique_ptr<base::Value> result2 =
      RunFunctionAndReturnValue(function2.get(), "[]");
  ASSERT_TRUE(result2);
  ASSERT_TRUE(result2->is_blob());
  ASSERT_EQ(generated_blob, result2->GetBlob());

  // Re-running should not change the secret even when force recreate is set.
  auto function3 = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::unique_ptr<base::Value> result3 =
      RunFunctionAndReturnValue(function3.get(), "[true]");
  ASSERT_TRUE(result3);
  ASSERT_TRUE(result3->is_blob());
  ASSERT_EQ(generated_blob, result3->GetBlob());

  const wchar_t kDefaultRegistryPath[] =
      L"SOFTWARE\\Google\\Endpoint Verification";
  const wchar_t kValueName[] = L"Safe Storage";

  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_CURRENT_USER, kDefaultRegistryPath, KEY_WRITE));
  // Mess up with the value.
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kValueName, 1337));

  // Re-running with no recreate enforcement should return an error.
  auto function4 = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::string error = RunFunctionAndReturnError(function4.get(), "[]");
  ASSERT_FALSE(error.empty());

  // Re=running should not change the secret even when force recreate is set.
  auto function5 = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::unique_ptr<base::Value> result5 =
      RunFunctionAndReturnValue(function5.get(), "[true]");
  ASSERT_TRUE(result5);
  ASSERT_TRUE(result5->is_blob());
  ASSERT_NE(generated_blob, result5->GetBlob());
}

#endif  // BUILDFLAG(IS_WIN)

using EnterpriseReportingPrivateGetDeviceInfoTest = ExtensionApiUnittest;

TEST_F(EnterpriseReportingPrivateGetDeviceInfoTest, GetDeviceInfo) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceInfoFunction>();
  std::unique_ptr<base::Value> device_info_value =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(device_info_value.get());
  enterprise_reporting_private::DeviceInfo info;
  ASSERT_TRUE(enterprise_reporting_private::DeviceInfo::Populate(
      *device_info_value, &info));
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ("macOS", info.os_name);
#elif BUILDFLAG(IS_WIN)
  EXPECT_EQ("windows", info.os_name);
  EXPECT_FALSE(info.device_model.empty());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(base::nix::kXdgCurrentDesktopEnvVar, "XFCE");
  EXPECT_EQ("linux", info.os_name);
#else
  // Verify a stub implementation.
  EXPECT_EQ("stubOS", info.os_name);
  EXPECT_EQ("0.0.0.0", info.os_version);
  EXPECT_EQ("security patch level", info.security_patch_level);
  EXPECT_EQ("midnightshift", info.device_host_name);
  EXPECT_EQ("topshot", info.device_model);
  EXPECT_EQ("twirlchange", info.serial_number);
  EXPECT_EQ(enterprise_reporting_private::SETTING_VALUE_ENABLED,
            info.screen_lock_secured);
  EXPECT_EQ(enterprise_reporting_private::SETTING_VALUE_DISABLED,
            info.disk_encrypted);
  ASSERT_EQ(1u, info.mac_addresses.size());
  EXPECT_EQ("00:00:00:00:00:00", info.mac_addresses[0]);
  EXPECT_EQ(*info.windows_machine_domain, "MACHINE_DOMAIN");
  EXPECT_EQ(*info.windows_user_domain, "USER_DOMAIN");
#endif
}

TEST_F(EnterpriseReportingPrivateGetDeviceInfoTest, GetDeviceInfoConversion) {
  // Verify that the conversion from a DeviceInfoFetcher result works,
  // regardless of platform.
  auto device_info_fetcher =
      enterprise_signals::DeviceInfoFetcher::CreateStubInstanceForTesting();

  enterprise_reporting_private::DeviceInfo info =
      EnterpriseReportingPrivateGetDeviceInfoFunction::ToDeviceInfo(
          device_info_fetcher->Fetch());
  EXPECT_EQ("stubOS", info.os_name);
  EXPECT_EQ("0.0.0.0", info.os_version);
  EXPECT_EQ("security patch level", info.security_patch_level);
  EXPECT_EQ("midnightshift", info.device_host_name);
  EXPECT_EQ("topshot", info.device_model);
  EXPECT_EQ("twirlchange", info.serial_number);
  EXPECT_EQ(enterprise_reporting_private::SETTING_VALUE_ENABLED,
            info.screen_lock_secured);
  EXPECT_EQ(enterprise_reporting_private::SETTING_VALUE_DISABLED,
            info.disk_encrypted);
  ASSERT_EQ(1u, info.mac_addresses.size());
  EXPECT_EQ("00:00:00:00:00:00", info.mac_addresses[0]);
  EXPECT_EQ(*info.windows_machine_domain, "MACHINE_DOMAIN");
  EXPECT_EQ(*info.windows_user_domain, "USER_DOMAIN");
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

class EnterpriseReportingPrivateGetContextInfoTest
    : public ExtensionApiUnittest {
 public:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    // Only used to set the right default BuiltInDnsClientEnabled preference
    // value according to the OS. DnsClient and DoH default preferences are
    // updated when the object is created, making the object unnecessary outside
    // this scope.
    StubResolverConfigReader stub_resolver_config_reader(
        g_browser_process->local_state());
  }

  enterprise_reporting_private::ContextInfo GetContextInfo() {
    auto function = base::MakeRefCounted<
        EnterpriseReportingPrivateGetContextInfoFunction>();
    std::unique_ptr<base::Value> context_info_value =
        RunFunctionAndReturnValue(function.get(), "[]");
    EXPECT_TRUE(context_info_value.get());

    enterprise_reporting_private::ContextInfo info;
    EXPECT_TRUE(enterprise_reporting_private::ContextInfo::Populate(
        *context_info_value, &info));

    return info;
  }

  bool BuiltInDnsClientPlatformDefault() {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    return true;
#else
    return false;
#endif
  }

  void ExpectDefaultChromeCleanupEnabled(
      const enterprise_reporting_private::ContextInfo& info) {
#if BUILDFLAG(IS_WIN)
    EXPECT_TRUE(*info.chrome_cleanup_enabled);
#else
    EXPECT_EQ(nullptr, info.chrome_cleanup_enabled.get());
#endif
  }
  void ExpectDefaultThirdPartyBlockingEnabled(
      const enterprise_reporting_private::ContextInfo& info) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    EXPECT_TRUE(*info.third_party_blocking_enabled);
#else
    EXPECT_EQ(info.third_party_blocking_enabled, nullptr);
#endif
  }
};

TEST_F(EnterpriseReportingPrivateGetContextInfoTest, NoSpecialContext) {
  // This tests the data returned by the API is correct when no special context
  // is present, ie no policies are set, the browser is unamanaged, etc.
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
      info.password_protection_warning_trigger);
  ExpectDefaultChromeCleanupEnabled(info);
  EXPECT_FALSE(info.chrome_remote_desktop_app_blocked);
  ExpectDefaultThirdPartyBlockingEnabled(info);
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
class EnterpriseReportingPrivateGetContextInfoThirdPartyBlockingTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<bool> {};

TEST_P(EnterpriseReportingPrivateGetContextInfoThirdPartyBlockingTest, Test) {
  bool policyValue = GetParam();

  g_browser_process->local_state()->SetBoolean(
      prefs::kThirdPartyBlockingEnabled, policyValue);

  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  ExpectDefaultChromeCleanupEnabled(info);
  EXPECT_FALSE(info.chrome_remote_desktop_app_blocked);
  EXPECT_EQ(policyValue, *info.third_party_blocking_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoThirdPartyBlockingTest,
    testing::Bool());
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

class EnterpriseReportingPrivateGetContextInfoSafeBrowsingTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {};

TEST_P(EnterpriseReportingPrivateGetContextInfoSafeBrowsingTest, Test) {
  std::tuple<bool, bool> params = GetParam();

  bool safe_browsing_enabled = std::get<0>(params);
  bool safe_browsing_enhanced_enabled = std::get<1>(params);

  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                    safe_browsing_enabled);
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                    safe_browsing_enhanced_enabled);
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);

  if (safe_browsing_enabled) {
    if (safe_browsing_enhanced_enabled)
      EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_ENHANCED,
                info.safe_browsing_protection_level);
    else
      EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
                info.safe_browsing_protection_level);
  } else {
    EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_DISABLED,
              info.safe_browsing_protection_level);
  }
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
      info.password_protection_warning_trigger);
  ExpectDefaultThirdPartyBlockingEnabled(info);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoSafeBrowsingTest,
    testing::Values(std::make_tuple(false, false),
                    std::make_tuple(true, false),
                    std::make_tuple(true, true)));

class EnterpriseReportingPrivateGetContextInfoBuiltInDnsClientTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<bool> {};

TEST_P(EnterpriseReportingPrivateGetContextInfoBuiltInDnsClientTest, Test) {
  bool policyValue = GetParam();

  g_browser_process->local_state()->SetBoolean(prefs::kBuiltInDnsClientEnabled,
                                               policyValue);

  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
            info.safe_browsing_protection_level);
  EXPECT_EQ(policyValue, info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
      info.password_protection_warning_trigger);
  ExpectDefaultThirdPartyBlockingEnabled(info);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoBuiltInDnsClientTest,
    testing::Bool());

class EnterpriseReportingPrivateGetContextPasswordProtectionWarningTrigger
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<
          enterprise_reporting_private::PasswordProtectionTrigger> {
 public:
  safe_browsing::PasswordProtectionTrigger MapPasswordProtectionTriggerToPolicy(
      enterprise_reporting_private::PasswordProtectionTrigger enumValue) {
    switch (enumValue) {
      case enterprise_reporting_private::
          PASSWORD_PROTECTION_TRIGGER_PASSWORD_PROTECTION_OFF:
        return safe_browsing::PASSWORD_PROTECTION_OFF;
      case enterprise_reporting_private::
          PASSWORD_PROTECTION_TRIGGER_PASSWORD_REUSE:
        return safe_browsing::PASSWORD_REUSE;
      case enterprise_reporting_private::
          PASSWORD_PROTECTION_TRIGGER_PHISHING_REUSE:
        return safe_browsing::PHISHING_REUSE;
      default:
        NOTREACHED();
        return safe_browsing::PASSWORD_PROTECTION_TRIGGER_MAX;
    }
  }
};

TEST_P(EnterpriseReportingPrivateGetContextPasswordProtectionWarningTrigger,
       Test) {
  enterprise_reporting_private::PasswordProtectionTrigger passwordTriggerValue =
      GetParam();

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger,
      MapPasswordProtectionTriggerToPolicy(passwordTriggerValue));
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  ExpectDefaultThirdPartyBlockingEnabled(info);
  EXPECT_EQ(passwordTriggerValue, info.password_protection_warning_trigger);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextPasswordProtectionWarningTrigger,
    testing::Values(enterprise_reporting_private::
                        PASSWORD_PROTECTION_TRIGGER_PASSWORD_PROTECTION_OFF,
                    enterprise_reporting_private::
                        PASSWORD_PROTECTION_TRIGGER_PASSWORD_REUSE,
                    enterprise_reporting_private::
                        PASSWORD_PROTECTION_TRIGGER_PHISHING_REUSE));

#if BUILDFLAG(IS_LINUX)
class EnterpriseReportingPrivateGetContextOSFirewallLinuxTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<
          enterprise_reporting_private::SettingValue> {
 public:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    ASSERT_TRUE(fake_appdata_dir_.CreateUniqueTempDir());
    file_path_ = fake_appdata_dir_.GetPath().Append("ufw.conf");
  }

  void ExpectDefaultPolicies(
      const enterprise_reporting_private::ContextInfo& info) {
    EXPECT_TRUE(info.browser_affiliation_ids.empty());
    EXPECT_TRUE(info.profile_affiliation_ids.empty());
    EXPECT_TRUE(info.on_file_attached_providers.empty());
    EXPECT_TRUE(info.on_file_downloaded_providers.empty());
    EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
    EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
              info.realtime_url_check_mode);
    EXPECT_TRUE(info.on_security_event_providers.empty());
    EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
    EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
              info.safe_browsing_protection_level);
    EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
              info.built_in_dns_client_enabled);
    EXPECT_EQ(
        enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
        info.password_protection_warning_trigger);
    ExpectDefaultChromeCleanupEnabled(info);
    EXPECT_FALSE(info.chrome_remote_desktop_app_blocked);
    ExpectDefaultThirdPartyBlockingEnabled(info);
  }

 protected:
  base::ScopedTempDir fake_appdata_dir_;
  base::FilePath file_path_;
};

TEST_F(EnterpriseReportingPrivateGetContextOSFirewallLinuxTest,
       NoFirewallFile) {
  // Refer to a non existent firewall config file
  enterprise_signals::ScopedUfwConfigPathForTesting scoped_path(
      file_path_.value().c_str());
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  ExpectDefaultPolicies(info);
  EXPECT_EQ(info.os_firewall,
            enterprise_reporting_private::SETTING_VALUE_UNKNOWN);
}

TEST_F(EnterpriseReportingPrivateGetContextOSFirewallLinuxTest, NoEnabledKey) {
  // Refer to a config file without the ENABLED=value key-value pair
  base::WriteFile(file_path_,
                  "#comment1\n#comment2\nLOGLEVEL=yes\nTESTKEY=yes\n");
  enterprise_signals::ScopedUfwConfigPathForTesting scoped_path(
      file_path_.value().c_str());
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  ExpectDefaultPolicies(info);
  EXPECT_EQ(info.os_firewall,
            enterprise_reporting_private::SETTING_VALUE_UNKNOWN);
}

TEST_P(EnterpriseReportingPrivateGetContextOSFirewallLinuxTest, Test) {
  enterprise_reporting_private::SettingValue os_firewall_value = GetParam();
  switch (os_firewall_value) {
    case enterprise_reporting_private::SETTING_VALUE_ENABLED:
      // File format to test if comments, empty lines and strings containing the
      // key are ignored
      base::WriteFile(file_path_,
                      "#ENABLED=no\nrandomtextENABLED=no\n  \nENABLED=yes\n");
      break;
    case enterprise_reporting_private::SETTING_VALUE_DISABLED:
      base::WriteFile(file_path_,
                      "#ENABLED=yes\nENABLEDrandomtext=yes\n  \nENABLED=no\n");
      break;
    case enterprise_reporting_private::SETTING_VALUE_UNKNOWN:
      // File content to test a value that isn't yes or no
      base::WriteFile(file_path_,
                      "#ENABLED=yes\nLOGLEVEL=yes\nENABLED=yesno\n");
      break;
    default:
      NOTREACHED();
  }

  enterprise_signals::ScopedUfwConfigPathForTesting scoped_path(
      file_path_.value().c_str());
  enterprise_reporting_private::ContextInfo info = GetContextInfo();
  ExpectDefaultPolicies(info);
  EXPECT_EQ(info.os_firewall, os_firewall_value);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextOSFirewallLinuxTest,
    testing::Values(enterprise_reporting_private::SETTING_VALUE_ENABLED,
                    enterprise_reporting_private::SETTING_VALUE_DISABLED,
                    enterprise_reporting_private::SETTING_VALUE_UNKNOWN));
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
class EnterpriseReportingPrivateGetContextInfoChromeCleanupTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<bool> {};

TEST_P(EnterpriseReportingPrivateGetContextInfoChromeCleanupTest, Test) {
  bool policy_value = GetParam();

  g_browser_process->local_state()->SetBoolean(prefs::kSwReporterEnabled,
                                               policy_value);

  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
      info.password_protection_warning_trigger);
  ExpectDefaultThirdPartyBlockingEnabled(info);
  EXPECT_EQ(policy_value, *info.chrome_cleanup_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoChromeCleanupTest,
    testing::Bool());
#endif  // BUILDFLAG(IS_WIN)

class EnterpriseReportingPrivateGetContextInfoChromeRemoteDesktopAppBlockedTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<const char*> {
 public:
  void SetURLBlockedPolicy(const std::string& url) {
    base::Value blockList(base::Value::Type::LIST);
    blockList.Append(base::Value(url));

    profile()->GetPrefs()->Set(policy::policy_prefs::kUrlBlocklist,
                               std::move(blockList));
  }
  void SetURLAllowedPolicy(const std::string& url) {
    base::Value allowList(base::Value::Type::LIST);
    allowList.Append(base::Value(url));

    profile()->GetPrefs()->Set(policy::policy_prefs::kUrlAllowlist,
                               std::move(allowList));
  }

  void ExpectDefaultPolicies(enterprise_reporting_private::ContextInfo& info) {
    EXPECT_TRUE(info.browser_affiliation_ids.empty());
    EXPECT_TRUE(info.profile_affiliation_ids.empty());
    EXPECT_TRUE(info.on_file_attached_providers.empty());
    EXPECT_TRUE(info.on_file_downloaded_providers.empty());
    EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
    EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
              info.realtime_url_check_mode);
    EXPECT_TRUE(info.on_security_event_providers.empty());
    EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
    EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
              info.safe_browsing_protection_level);
    EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
              info.built_in_dns_client_enabled);
    EXPECT_EQ(
        enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
        info.password_protection_warning_trigger);
    ExpectDefaultChromeCleanupEnabled(info);
    ExpectDefaultThirdPartyBlockingEnabled(info);
  }
};

TEST_P(
    EnterpriseReportingPrivateGetContextInfoChromeRemoteDesktopAppBlockedTest,
    BlockedURL) {
  SetURLBlockedPolicy(GetParam());

  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  ExpectDefaultPolicies(info);
  EXPECT_TRUE(info.chrome_remote_desktop_app_blocked);
}

TEST_P(
    EnterpriseReportingPrivateGetContextInfoChromeRemoteDesktopAppBlockedTest,
    AllowedURL) {
  SetURLAllowedPolicy(GetParam());

  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  ExpectDefaultPolicies(info);
  EXPECT_FALSE(info.chrome_remote_desktop_app_blocked);
}

TEST_P(
    EnterpriseReportingPrivateGetContextInfoChromeRemoteDesktopAppBlockedTest,
    BlockedAndAllowedURL) {
  SetURLBlockedPolicy(GetParam());
  SetURLAllowedPolicy(GetParam());

  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  ExpectDefaultPolicies(info);
  EXPECT_FALSE(info.chrome_remote_desktop_app_blocked);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoChromeRemoteDesktopAppBlockedTest,
    testing::Values("https://remotedesktop.google.com",
                    "https://remotedesktop.corp.google.com",
                    "corp.google.com",
                    "google.com",
                    "https://*"));

#if BUILDFLAG(IS_WIN)
class EnterpriseReportingPrivateGetContextInfoOSFirewallTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<SettingValue> {
 public:
  EnterpriseReportingPrivateGetContextInfoOSFirewallTest()
      : enabled_(VARIANT_TRUE) {}

 protected:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    HRESULT hr = CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&firewall_policy_));
    EXPECT_FALSE(FAILED(hr));

    long profile_types = 0;
    hr = firewall_policy_->get_CurrentProfileTypes(&profile_types);
    EXPECT_FALSE(FAILED(hr));

    // Setting the firewall for each active profile
    const NET_FW_PROFILE_TYPE2 kProfileTypes[] = {NET_FW_PROFILE2_PUBLIC,
                                                  NET_FW_PROFILE2_PRIVATE,
                                                  NET_FW_PROFILE2_DOMAIN};
    for (size_t i = 0; i < std::size(kProfileTypes); ++i) {
      if ((profile_types & kProfileTypes[i]) != 0) {
        hr = firewall_policy_->get_FirewallEnabled(kProfileTypes[i], &enabled_);
        EXPECT_FALSE(FAILED(hr));
        active_profile_ = kProfileTypes[i];
        hr = firewall_policy_->put_FirewallEnabled(
            kProfileTypes[i], firewall_value_ == SettingValue::ENABLED
                                  ? VARIANT_TRUE
                                  : VARIANT_FALSE);
        EXPECT_FALSE(FAILED(hr));
        break;
      }
    }
  }

  void TearDown() override {
    // Resetting the firewall to its initial state
    HRESULT hr =
        firewall_policy_->put_FirewallEnabled(active_profile_, enabled_);
    EXPECT_FALSE(FAILED(hr));
  }

  extensions::api::enterprise_reporting_private::SettingValue
  ToInfoSettingValue(enterprise_signals::SettingValue value) {
    switch (value) {
      case SettingValue::DISABLED:
        return extensions::api::enterprise_reporting_private::
            SETTING_VALUE_DISABLED;
      case SettingValue::ENABLED:
        return extensions::api::enterprise_reporting_private::
            SETTING_VALUE_ENABLED;
      default:
        NOTREACHED();
        return extensions::api::enterprise_reporting_private::
            SETTING_VALUE_UNKNOWN;
    }
  }
  Microsoft::WRL::ComPtr<INetFwPolicy2> firewall_policy_;
  SettingValue firewall_value_ = GetParam();
  VARIANT_BOOL enabled_;
  NET_FW_PROFILE_TYPE2 active_profile_;
};

TEST_P(EnterpriseReportingPrivateGetContextInfoOSFirewallTest, Test) {
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
      info.password_protection_warning_trigger);
  ExpectDefaultChromeCleanupEnabled(info);
  EXPECT_FALSE(info.chrome_remote_desktop_app_blocked);
  ExpectDefaultThirdPartyBlockingEnabled(info);
  EXPECT_EQ(ToInfoSettingValue(firewall_value_), info.os_firewall);
}

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseReportingPrivateGetContextInfoOSFirewallTest,
                         testing::Values(SettingValue::DISABLED,
                                         SettingValue::ENABLED));

#endif  // BUILDFLAG(IS_WIN)

class EnterpriseReportingPrivateGetContextInfoRealTimeURLCheckTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<bool> {
 public:
  EnterpriseReportingPrivateGetContextInfoRealTimeURLCheckTest() {
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("fake-token"));
  }

  bool url_check_enabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoRealTimeURLCheckTest,
    testing::Bool());

TEST_P(EnterpriseReportingPrivateGetContextInfoRealTimeURLCheckTest, Test) {
  profile()->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode,
      url_check_enabled() ? safe_browsing::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED
                          : safe_browsing::REAL_TIME_CHECK_DISABLED);
  profile()->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  if (url_check_enabled()) {
    EXPECT_EQ(enterprise_reporting_private::
                  REALTIME_URL_CHECK_MODE_ENABLED_MAIN_FRAME,
              info.realtime_url_check_mode);
  } else {
    EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
              info.realtime_url_check_mode);
  }

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
      info.password_protection_warning_trigger);
  ExpectDefaultThirdPartyBlockingEnabled(info);
}

#if BUILDFLAG(IS_CHROMEOS)
// Test for API enterprise.reportingPrivate.enqueueRecord
class EnterpriseReportingPrivateEnqueueRecordFunctionTest
    : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateEnqueueRecordFunctionTest() = default;

  EnterpriseReportingPrivateEnqueueRecordFunctionTest(
      const EnterpriseReportingPrivateEnqueueRecordFunctionTest&) = delete;
  EnterpriseReportingPrivateEnqueueRecordFunctionTest& operator=(
      const EnterpriseReportingPrivateEnqueueRecordFunctionTest&) = delete;

  static constexpr char kNoError[] = "";
  static constexpr char kTestDMTokenValue[] = "test_dm_token_value";

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    ::chromeos::MissiveClient::InitializeFake();
    function_ =
        base::MakeRefCounted<EnterpriseReportingPrivateEnqueueRecordFunction>();
    // Set up a Record to attach to an EnqueueRecordRequest
    base::Value data{base::Value::Type::DICTIONARY};
    data.SetKey("TEST_KEY", base::Value("TEST_VALUE"));
    std::string serialized_data;
    ASSERT_TRUE(base::JSONWriter::Write(data, &serialized_data));
    reporting::Record record;
    record.set_data(serialized_data);
    record.set_destination(reporting::Destination::TELEMETRY_METRIC);
    serialized_record_data_.resize(record.SerializeAsString().size());
    ASSERT_TRUE(record.SerializeToArray(serialized_record_data_.data(),
                                        serialized_record_data_.size()));
  }

  void TearDown() override {
    function_.reset();
    ::chromeos::MissiveClient::Shutdown();
    ExtensionApiUnittest::TearDown();
  }

 protected:
  std::vector<uint8_t> serialized_record_data_;
  scoped_refptr<extensions::EnterpriseReportingPrivateEnqueueRecordFunction>
      function_;
};

TEST_F(EnterpriseReportingPrivateEnqueueRecordFunctionTest,
       ValidRecordSuccessfullyEnqueued) {
  function_->SetProfileIsAffiliatedForTesting(/*is_affiliated=*/true);

  api::enterprise_reporting_private::EnqueueRecordRequest
      enqueue_record_request;
  enqueue_record_request.record_data = serialized_record_data_;
  enqueue_record_request.priority = reporting::Priority::BACKGROUND_BATCH;
  enqueue_record_request.event_type =
      api::enterprise_reporting_private::EventType::EVENT_TYPE_USER;

  std::unique_ptr<base::ListValue> params = std::make_unique<base::ListValue>();
  params->Append(enqueue_record_request.ToValue());

  // Set up DM token
  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidTokenForTesting(kTestDMTokenValue));

  extension_function_test_utils::RunFunction(function_.get(), std::move(params),
                                             browser(),
                                             extensions::api_test_utils::NONE);
  EXPECT_EQ(function_->GetError(), kNoError);
}

TEST_F(EnterpriseReportingPrivateEnqueueRecordFunctionTest,
       InvalidPriorityReturnsError) {
  function_->SetProfileIsAffiliatedForTesting(true);

  api::enterprise_reporting_private::EnqueueRecordRequest
      enqueue_record_request;
  enqueue_record_request.record_data = serialized_record_data_;

  // Set priority to invalid enum value
  enqueue_record_request.priority = -1;

  enqueue_record_request.event_type =
      api::enterprise_reporting_private::EventType::EVENT_TYPE_USER;

  std::unique_ptr<base::ListValue> params = std::make_unique<base::ListValue>();
  params->Append(enqueue_record_request.ToValue());

  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidTokenForTesting(kTestDMTokenValue));

  extension_function_test_utils::RunFunction(function_.get(), std::move(params),
                                             browser(),
                                             extensions::api_test_utils::NONE);

  EXPECT_EQ(function_->GetError(),
            EnterpriseReportingPrivateEnqueueRecordFunction::
                kErrorInvalidEnqueueRecordRequest);
}

TEST_F(EnterpriseReportingPrivateEnqueueRecordFunctionTest,
       NonAffiliatedUserReturnsError) {
  function_->SetProfileIsAffiliatedForTesting(false);

  api::enterprise_reporting_private::EnqueueRecordRequest
      enqueue_record_request;
  enqueue_record_request.record_data = serialized_record_data_;

  enqueue_record_request.priority = reporting::Priority::BACKGROUND_BATCH;

  enqueue_record_request.event_type =
      api::enterprise_reporting_private::EventType::EVENT_TYPE_USER;

  std::unique_ptr<base::ListValue> params = std::make_unique<base::ListValue>();
  params->Append(enqueue_record_request.ToValue());

  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidTokenForTesting(kTestDMTokenValue));

  extension_function_test_utils::RunFunction(function_.get(), std::move(params),
                                             browser(),
                                             extensions::api_test_utils::NONE);

  EXPECT_EQ(function_->GetError(),
            EnterpriseReportingPrivateEnqueueRecordFunction::
                kErrorProfileNotAffiliated);
}

TEST_F(EnterpriseReportingPrivateEnqueueRecordFunctionTest,
       InvalidDMTokenReturnsError) {
  function_->SetProfileIsAffiliatedForTesting(true);
  api::enterprise_reporting_private::EnqueueRecordRequest
      enqueue_record_request;
  enqueue_record_request.record_data = serialized_record_data_;
  enqueue_record_request.priority = reporting::Priority::BACKGROUND_BATCH;
  enqueue_record_request.event_type =
      api::enterprise_reporting_private::EventType::EVENT_TYPE_USER;

  std::unique_ptr<base::ListValue> params = std::make_unique<base::ListValue>();
  params->Append(enqueue_record_request.ToValue());

  // Set up invalid DM token
  policy::SetDMTokenForTesting(policy::DMToken::CreateInvalidTokenForTesting());

  extension_function_test_utils::RunFunction(function_.get(), std::move(params),
                                             browser(),
                                             extensions::api_test_utils::NONE);

  EXPECT_EQ(function_->GetError(),
            EnterpriseReportingPrivateEnqueueRecordFunction::
                kErrorCannotAssociateRecordWithUser);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
