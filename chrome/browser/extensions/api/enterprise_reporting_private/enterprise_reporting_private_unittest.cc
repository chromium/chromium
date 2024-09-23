// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/common/extensions/api/enterprise_reporting_private.h"

#include <tuple>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "chrome/browser/enterprise/signals/signals_common.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/component_updater/pref_names.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/missive/fake_missive_client.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <netfw.h>
#include <shlobj.h>
#include <wrl/client.h>

#include "base/test/test_reg_util_win.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/signals/signals_aggregator_factory.h"
#include "components/device_signals/core/browser/mock_signals_aggregator.h"  // nogncheck
#include "components/device_signals/core/browser/signals_aggregator.h"  // nogncheck
#include "components/device_signals/core/browser/signals_types.h"  // nogncheck
#include "components/device_signals/core/browser/user_context.h"   // nogncheck
#include "components/device_signals/core/common/common_types.h"    // nogncheck
#include "components/device_signals/core/common/signals_constants.h"  // nogncheck
#include "components/device_signals/core/common/signals_features.h"  // nogncheck
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/nix/xdg_util.h"
#endif

namespace enterprise_reporting_private =
    ::extensions::api::enterprise_reporting_private;

using SettingValue = enterprise_signals::SettingValue;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::WithArgs;

namespace extensions {

std::unique_ptr<KeyedService> CreateProfileIDService(
    content::BrowserContext* context) {
  static constexpr char kFakeProfileID[] = "fake-profile-id";
  return std::make_unique<enterprise::ProfileIdService>(kFakeProfileID);
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_LINUX)

constexpr char kNoError[] = "";

#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_CHROMEOS)

namespace {

constexpr char kFakeClientId[] = "fake-client-id";

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
    storage_.ResetForTesting();
  }

 private:
  policy::FakeBrowserDMTokenStorage storage_;
};

TEST_F(EnterpriseReportingPrivateGetDeviceIdTest, GetDeviceId) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceIdFunction>();
  SetClientId(kFakeClientId);
  std::optional<base::Value> id =
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
  base::Value::List values;
  values.Append("a");
  values.Append(base::Value::BlobStorage({1, 2, 3}));
  api_test_utils::RunFunction(
      function.get(), std::move(values),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);
  ASSERT_TRUE(function->GetResultListForTest());
  EXPECT_EQ(0u, function->GetResultListForTest()->size());
  EXPECT_TRUE(function->GetError().empty());
}

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, DeviceDataMissing) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  base::Value::List values;
  values.Append("b");
  api_test_utils::RunFunction(
      function.get(), std::move(values),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);
  ASSERT_TRUE(function->GetResultListForTest());
  EXPECT_EQ(1u, function->GetResultListForTest()->size());
  EXPECT_TRUE(function->GetError().empty());

  const base::Value& single_result = (*function->GetResultListForTest())[0];
  ASSERT_TRUE(single_result.is_blob());
  EXPECT_EQ(base::Value::BlobStorage(), single_result.GetBlob());
}

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, DeviceBadId) {
  auto set_function =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  base::Value::List set_values;
  set_values.Append("a/b");
  set_values.Append(base::Value::BlobStorage({1, 2, 3}));
  api_test_utils::RunFunction(
      set_function.get(), std::move(set_values),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);
  ASSERT_TRUE(set_function->GetError().empty());

  // Try to read the directory as a file and should fail.
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  base::Value::List values;
  values.Append("a");
  api_test_utils::RunFunction(
      function.get(), std::move(values),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);
  ASSERT_TRUE(function->GetResultListForTest());
  EXPECT_EQ(0u, function->GetResultListForTest()->size());
  EXPECT_FALSE(function->GetError().empty());
}

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, RetrieveDeviceData) {
  auto set_function =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  base::Value::List set_values;
  set_values.Append("c");
  set_values.Append(base::Value::BlobStorage({1, 2, 3}));
  api_test_utils::RunFunction(
      set_function.get(), std::move(set_values),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);
  ASSERT_TRUE(set_function->GetError().empty());

  auto get_function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  base::Value::List values;
  values.Append("c");
  api_test_utils::RunFunction(
      get_function.get(), std::move(values),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);
  ASSERT_TRUE(get_function->GetResultListForTest());
  const base::Value& single_result = (*get_function->GetResultListForTest())[0];
  EXPECT_TRUE(get_function->GetError().empty());
  ASSERT_TRUE(single_result.is_blob());
  EXPECT_EQ(base::Value::BlobStorage({1, 2, 3}), single_result.GetBlob());

  // Clear the data and check that it is gone.
  auto set_function2 =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  base::Value::List reset_values;
  reset_values.Append("c");
  api_test_utils::RunFunction(
      set_function2.get(), std::move(reset_values),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);
  ASSERT_TRUE(set_function2->GetError().empty());

  auto get_function2 =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  base::Value::List values2;
  values2.Append("c");
  api_test_utils::RunFunction(
      get_function2.get(), std::move(values2),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);
  ASSERT_TRUE(get_function2->GetResultListForTest());
  EXPECT_EQ(1u, get_function2->GetResultListForTest()->size());
  EXPECT_TRUE(get_function2->GetError().empty());

  const base::Value& single_result2 =
      (*get_function2->GetResultListForTest())[0];
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
  std::optional<base::Value> result1 =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(result1);
  ASSERT_TRUE(result1->is_blob());
  auto generated_blob = result1->GetBlob();

  // Re-running should not change the secret.
  auto function2 = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::optional<base::Value> result2 =
      RunFunctionAndReturnValue(function2.get(), "[]");
  ASSERT_TRUE(result2);
  ASSERT_TRUE(result2->is_blob());
  ASSERT_EQ(generated_blob, result2->GetBlob());

  // Re-running should not change the secret even when force recreate is set.
  auto function3 = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::optional<base::Value> result3 =
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
  std::optional<base::Value> result5 =
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
  std::optional<base::Value> device_info_value =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(device_info_value);
  ASSERT_TRUE(device_info_value->is_dict());
  auto info = enterprise_reporting_private::DeviceInfo::FromValue(
      device_info_value->GetDict());
  ASSERT_TRUE(info);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ("macOS", info->os_name);
#elif BUILDFLAG(IS_WIN)
  EXPECT_EQ("windows", info->os_name);
  EXPECT_FALSE(info->device_model.empty());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(base::nix::kXdgCurrentDesktopEnvVar, "XFCE");
  EXPECT_EQ("linux", info->os_name);
#else
  // Verify a stub implementation.
  EXPECT_EQ("stubOS", info->os_name);
  EXPECT_EQ("0.0.0.0", info->os_version);
  EXPECT_EQ("security patch level", info->security_patch_level);
  EXPECT_EQ("midnightshift", info->device_host_name);
  EXPECT_EQ("topshot", info->device_model);
  EXPECT_EQ("twirlchange", info->serial_number);
  EXPECT_EQ(enterprise_reporting_private::SettingValue::kEnabled,
            info->screen_lock_secured);
  EXPECT_EQ(enterprise_reporting_private::SettingValue::kDisabled,
            info->disk_encrypted);
  ASSERT_EQ(1u, info->mac_addresses.size());
  EXPECT_EQ("00:00:00:00:00:00", info->mac_addresses[0]);
  EXPECT_EQ(*info->windows_machine_domain, "MACHINE_DOMAIN");
  EXPECT_EQ(*info->windows_user_domain, "USER_DOMAIN");
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
  EXPECT_EQ(enterprise_reporting_private::SettingValue::kEnabled,
            info.screen_lock_secured);
  EXPECT_EQ(enterprise_reporting_private::SettingValue::kDisabled,
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
    enterprise::ProfileIdServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&CreateProfileIDService));
  }

  enterprise_reporting_private::ContextInfo GetContextInfo() {
    auto function = base::MakeRefCounted<
        EnterpriseReportingPrivateGetContextInfoFunction>();
    std::optional<base::Value> context_info_value =
        RunFunctionAndReturnValue(function.get(), "[]");
    EXPECT_TRUE(context_info_value);
    EXPECT_TRUE(context_info_value->is_dict());

    auto info = enterprise_reporting_private::ContextInfo::FromValue(
        context_info_value->GetDict());
    EXPECT_TRUE(info);

    return std::move(info).value_or(
        enterprise_reporting_private::ContextInfo());
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

TEST_F(EnterpriseReportingPrivateGetContextInfoTest, NoSpecialContext) {
  // This tests the data returned by the API is correct when no special context
  // is present, ie no policies are set, the browser is unamanaged, etc.
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kStandard,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PasswordProtectionTrigger::kPolicyUnset,
      info.password_protection_warning_trigger);
  EXPECT_FALSE(info.chrome_remote_desktop_app_blocked);
  ExpectDefaultThirdPartyBlockingEnabled(info);
  EXPECT_TRUE(info.enterprise_profile_id);
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
  EXPECT_EQ(enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kStandard,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
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
  EXPECT_EQ(enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);

  if (!safe_browsing_enabled) {
    EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kDisabled,
              info.safe_browsing_protection_level);
  } else if (safe_browsing_enhanced_enabled) {
    EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kEnhanced,
              info.safe_browsing_protection_level);
  } else {
    EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kStandard,
              info.safe_browsing_protection_level);
  }
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PasswordProtectionTrigger::kPolicyUnset,
      info.password_protection_warning_trigger);
  ExpectDefaultThirdPartyBlockingEnabled(info);
  EXPECT_TRUE(info.enterprise_profile_id);
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
  EXPECT_EQ(enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kStandard,
            info.safe_browsing_protection_level);
  EXPECT_EQ(policyValue, info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PasswordProtectionTrigger::kPolicyUnset,
      info.password_protection_warning_trigger);
  ExpectDefaultThirdPartyBlockingEnabled(info);
  EXPECT_TRUE(info.enterprise_profile_id);
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
      case enterprise_reporting_private::PasswordProtectionTrigger::
          kPasswordProtectionOff:
        return safe_browsing::PASSWORD_PROTECTION_OFF;
      case enterprise_reporting_private::PasswordProtectionTrigger::
          kPasswordReuse:
        return safe_browsing::PASSWORD_REUSE;
      case enterprise_reporting_private::PasswordProtectionTrigger::
          kPhishingReuse:
        return safe_browsing::PHISHING_REUSE;
      default:
        NOTREACHED_IN_MIGRATION();
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
  EXPECT_EQ(enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kStandard,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  ExpectDefaultThirdPartyBlockingEnabled(info);
  EXPECT_EQ(passwordTriggerValue, info.password_protection_warning_trigger);
  EXPECT_TRUE(info.enterprise_profile_id);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextPasswordProtectionWarningTrigger,
    testing::Values(
        enterprise_reporting_private::PasswordProtectionTrigger::
            kPasswordProtectionOff,
        enterprise_reporting_private::PasswordProtectionTrigger::kPasswordReuse,
        enterprise_reporting_private::PasswordProtectionTrigger::
            kPhishingReuse));

#if BUILDFLAG(IS_LINUX)
class EnterpriseReportingPrivateGetContextOSFirewallLinuxTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<
          enterprise_reporting_private::SettingValue> {
 public:
  void SetUp() override {
    EnterpriseReportingPrivateGetContextInfoTest::SetUp();
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
    EXPECT_EQ(enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled,
              info.realtime_url_check_mode);
    EXPECT_TRUE(info.on_security_event_providers.empty());
    EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
    EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kStandard,
              info.safe_browsing_protection_level);
    EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
              info.built_in_dns_client_enabled);
    EXPECT_EQ(
        enterprise_reporting_private::PasswordProtectionTrigger::kPolicyUnset,
        info.password_protection_warning_trigger);
    EXPECT_FALSE(info.chrome_remote_desktop_app_blocked);
    ExpectDefaultThirdPartyBlockingEnabled(info);
    EXPECT_TRUE(info.enterprise_profile_id);
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
            enterprise_reporting_private::SettingValue::kUnknown);
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
            enterprise_reporting_private::SettingValue::kUnknown);
}

TEST_P(EnterpriseReportingPrivateGetContextOSFirewallLinuxTest, Test) {
  enterprise_reporting_private::SettingValue os_firewall_value = GetParam();
  switch (os_firewall_value) {
    case enterprise_reporting_private::SettingValue::kEnabled:
      // File format to test if comments, empty lines and strings containing the
      // key are ignored
      base::WriteFile(file_path_,
                      "#ENABLED=no\nrandomtextENABLED=no\n  \nENABLED=yes\n");
      break;
    case enterprise_reporting_private::SettingValue::kDisabled:
      base::WriteFile(file_path_,
                      "#ENABLED=yes\nENABLEDrandomtext=yes\n  \nENABLED=no\n");
      break;
    case enterprise_reporting_private::SettingValue::kUnknown:
      // File content to test a value that isn't yes or no
      base::WriteFile(file_path_,
                      "#ENABLED=yes\nLOGLEVEL=yes\nENABLED=yesno\n");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
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
    testing::Values(enterprise_reporting_private::SettingValue::kEnabled,
                    enterprise_reporting_private::SettingValue::kDisabled,
                    enterprise_reporting_private::SettingValue::kUnknown));
#endif  // BUILDFLAG(IS_LINUX)

class EnterpriseReportingPrivateGetContextInfoChromeRemoteDesktopAppBlockedTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<const char*> {
 public:
  void SetURLBlockedPolicy(const std::string& url) {
    base::Value::List blocklist;
    blocklist.Append(url);

    profile()->GetPrefs()->SetList(policy::policy_prefs::kUrlBlocklist,
                                   std::move(blocklist));
  }
  void SetURLAllowedPolicy(const std::string& url) {
    base::Value::List allowlist;
    allowlist.Append(url);

    profile()->GetPrefs()->SetList(policy::policy_prefs::kUrlAllowlist,
                                   std::move(allowlist));
  }

  void ExpectDefaultPolicies(
      const enterprise_reporting_private::ContextInfo& info) {
    EXPECT_TRUE(info.browser_affiliation_ids.empty());
    EXPECT_TRUE(info.profile_affiliation_ids.empty());
    EXPECT_TRUE(info.on_file_attached_providers.empty());
    EXPECT_TRUE(info.on_file_downloaded_providers.empty());
    EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
    EXPECT_EQ(enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled,
              info.realtime_url_check_mode);
    EXPECT_TRUE(info.on_security_event_providers.empty());
    EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
    EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kStandard,
              info.safe_browsing_protection_level);
    EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
              info.built_in_dns_client_enabled);
    EXPECT_EQ(
        enterprise_reporting_private::PasswordProtectionTrigger::kPolicyUnset,
        info.password_protection_warning_trigger);
    ExpectDefaultThirdPartyBlockingEnabled(info);
    EXPECT_TRUE(info.enterprise_profile_id);
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
    if (!::IsUserAnAdmin()) {
      // INetFwPolicy2::put_FirewallEnabled fails for non-admin users.
      GTEST_SKIP() << "This test must be run by an admin user";
    }
    EnterpriseReportingPrivateGetContextInfoTest::SetUp();
    HRESULT hr = CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&firewall_policy_));
    EXPECT_GE(hr, 0);

    long profile_types = 0;
    hr = firewall_policy_->get_CurrentProfileTypes(&profile_types);
    EXPECT_GE(hr, 0);

    // Setting the firewall for each active profile
    const NET_FW_PROFILE_TYPE2 kProfileTypes[] = {NET_FW_PROFILE2_PUBLIC,
                                                  NET_FW_PROFILE2_PRIVATE,
                                                  NET_FW_PROFILE2_DOMAIN};
    for (size_t i = 0; i < std::size(kProfileTypes); ++i) {
      if ((profile_types & kProfileTypes[i]) != 0) {
        hr = firewall_policy_->get_FirewallEnabled(kProfileTypes[i], &enabled_);
        EXPECT_GE(hr, 0);
        active_profile_ = kProfileTypes[i];
        hr = firewall_policy_->put_FirewallEnabled(
            kProfileTypes[i], firewall_value_ == SettingValue::ENABLED
                                  ? VARIANT_TRUE
                                  : VARIANT_FALSE);
        EXPECT_GE(hr, 0);
        break;
      }
    }
  }

  void TearDown() override {
    if (!::IsUserAnAdmin()) {
      // Test already skipped in `SetUp`.
      return;
    }
    // Resetting the firewall to its initial state
    HRESULT hr =
        firewall_policy_->put_FirewallEnabled(active_profile_, enabled_);
    EXPECT_GE(hr, 0);
    EnterpriseReportingPrivateGetContextInfoTest::TearDown();
  }

  extensions::api::enterprise_reporting_private::SettingValue
  ToInfoSettingValue(enterprise_signals::SettingValue value) {
    switch (value) {
      case SettingValue::DISABLED:
        return extensions::api::enterprise_reporting_private::SettingValue::
            kDisabled;
      case SettingValue::ENABLED:
        return extensions::api::enterprise_reporting_private::SettingValue::
            kEnabled;
      default:
        NOTREACHED_IN_MIGRATION();
        return extensions::api::enterprise_reporting_private::SettingValue::
            kUnknown;
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
  EXPECT_EQ(enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kStandard,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PasswordProtectionTrigger::kPolicyUnset,
      info.password_protection_warning_trigger);
  EXPECT_FALSE(info.chrome_remote_desktop_app_blocked);
  ExpectDefaultThirdPartyBlockingEnabled(info);
  EXPECT_EQ(ToInfoSettingValue(firewall_value_), info.os_firewall);
  EXPECT_TRUE(info.enterprise_profile_id);
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
        policy::DMToken::CreateValidToken("fake-token"));
  }

  bool url_check_enabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoRealTimeURLCheckTest,
    testing::Bool());

TEST_P(EnterpriseReportingPrivateGetContextInfoRealTimeURLCheckTest, Test) {
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      url_check_enabled()
          ? enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED
          : enterprise_connectors::REAL_TIME_CHECK_DISABLED);
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  if (url_check_enabled()) {
    EXPECT_EQ(
        enterprise_reporting_private::RealtimeUrlCheckMode::kEnabledMainFrame,
        info.realtime_url_check_mode);
  } else {
    EXPECT_EQ(enterprise_reporting_private::RealtimeUrlCheckMode::kDisabled,
              info.realtime_url_check_mode);
  }

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SafeBrowsingLevel::kStandard,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PasswordProtectionTrigger::kPolicyUnset,
      info.password_protection_warning_trigger);
  ExpectDefaultThirdPartyBlockingEnabled(info);
  EXPECT_TRUE(info.enterprise_profile_id);
}

#if BUILDFLAG(IS_CHROMEOS)

// Test for API enterprise.reportingPrivate.enqueueRecord
class EnterpriseReportingPrivateEnqueueRecordFunctionTest
    : public ExtensionApiUnittest {
 protected:
  static constexpr char kTestDMTokenValue[] = "test_dm_token_value";

  EnterpriseReportingPrivateEnqueueRecordFunctionTest() = default;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    ::chromeos::MissiveClient::InitializeFake();
    function_ =
        base::MakeRefCounted<EnterpriseReportingPrivateEnqueueRecordFunction>();
    const auto record = GetTestRecord();
    serialized_record_data_.resize(record.ByteSizeLong());
    ASSERT_TRUE(record.SerializeToArray(serialized_record_data_.data(),
                                        serialized_record_data_.size()));
  }

  void TearDown() override {
    function_.reset();
    ::chromeos::MissiveClient::Shutdown();
    ExtensionApiUnittest::TearDown();
  }

  ::reporting::Record GetTestRecord() const {
    base::Value::Dict data;
    data.Set("TEST_KEY", base::Value("TEST_VALUE"));
    std::string serialized_data;
    DCHECK(base::JSONWriter::Write(data, &serialized_data));

    ::reporting::Record record;
    record.set_data(serialized_data);
    record.set_destination(::reporting::Destination::TELEMETRY_METRIC);
    record.set_timestamp_us(base::Time::Now().InMillisecondsSinceUnixEpoch() *
                            base::Time::kMicrosecondsPerMillisecond);

    return record;
  }

  void VerifyNoRecordsEnqueued(::reporting::Priority priority =
                                   ::reporting::Priority::BACKGROUND_BATCH) {
    ::chromeos::MissiveClient::TestInterface* const missive_test_interface =
        ::chromeos::MissiveClient::Get()->GetTestInterface();
    ASSERT_TRUE(missive_test_interface);

    const std::vector<::reporting::Record>& records =
        missive_test_interface->GetEnqueuedRecords(priority);

    ASSERT_THAT(records, IsEmpty());
  }

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
  enqueue_record_request.priority = ::reporting::Priority::BACKGROUND_BATCH;
  enqueue_record_request.event_type =
      api::enterprise_reporting_private::EventType::kUser;

  base::Value::List params;
  params.Append(enqueue_record_request.ToValue());

  // Set up DM token
  const auto dm_token = policy::DMToken::CreateValidToken(kTestDMTokenValue);
  policy::SetDMTokenForTesting(dm_token);

  api_test_utils::RunFunction(
      function_.get(), std::move(params),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(function_->GetError(), kNoError);

  ::chromeos::MissiveClient::TestInterface* const missive_test_interface =
      ::chromeos::MissiveClient::Get()->GetTestInterface();
  ASSERT_TRUE(missive_test_interface);

  const std::vector<::reporting::Record>& background_batch_records =
      missive_test_interface->GetEnqueuedRecords(
          ::reporting::Priority::BACKGROUND_BATCH);

  ASSERT_THAT(background_batch_records, SizeIs(1));
  EXPECT_THAT(background_batch_records[0].destination(),
              Eq(::reporting::Destination::TELEMETRY_METRIC));
  EXPECT_THAT(background_batch_records[0].dm_token(), StrEq(dm_token.value()));
  EXPECT_THAT(background_batch_records[0].data(),
              StrEq(GetTestRecord().data()));
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
      api::enterprise_reporting_private::EventType::kUser;

  base::Value::List params;
  params.Append(enqueue_record_request.ToValue());

  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidToken(kTestDMTokenValue));

  api_test_utils::RunFunction(
      function_.get(), std::move(params),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);

  EXPECT_EQ(function_->GetError(),
            EnterpriseReportingPrivateEnqueueRecordFunction::
                kErrorInvalidEnqueueRecordRequest);

  VerifyNoRecordsEnqueued();
}

TEST_F(EnterpriseReportingPrivateEnqueueRecordFunctionTest,
       NonAffiliatedUserReturnsError) {
  function_->SetProfileIsAffiliatedForTesting(false);

  api::enterprise_reporting_private::EnqueueRecordRequest
      enqueue_record_request;
  enqueue_record_request.record_data = serialized_record_data_;

  enqueue_record_request.priority = ::reporting::Priority::BACKGROUND_BATCH;

  enqueue_record_request.event_type =
      api::enterprise_reporting_private::EventType::kUser;

  base::Value::List params;
  params.Append(enqueue_record_request.ToValue());

  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidToken(kTestDMTokenValue));

  api_test_utils::RunFunction(
      function_.get(), std::move(params),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);

  EXPECT_EQ(function_->GetError(),
            EnterpriseReportingPrivateEnqueueRecordFunction::
                kErrorProfileNotAffiliated);

  VerifyNoRecordsEnqueued();
}

TEST_F(EnterpriseReportingPrivateEnqueueRecordFunctionTest,
       InvalidDMTokenReturnsError) {
  function_->SetProfileIsAffiliatedForTesting(true);
  api::enterprise_reporting_private::EnqueueRecordRequest
      enqueue_record_request;
  enqueue_record_request.record_data = serialized_record_data_;
  enqueue_record_request.priority = ::reporting::Priority::BACKGROUND_BATCH;
  enqueue_record_request.event_type =
      api::enterprise_reporting_private::EventType::kUser;

  base::Value::List params;
  params.Append(enqueue_record_request.ToValue());

  // Set up invalid DM token
  policy::SetDMTokenForTesting(policy::DMToken::CreateInvalidToken());

  api_test_utils::RunFunction(
      function_.get(), std::move(params),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);

  EXPECT_EQ(function_->GetError(),
            EnterpriseReportingPrivateEnqueueRecordFunction::
                kErrorCannotAssociateRecordWithUser);

  VerifyNoRecordsEnqueued();
}

TEST_F(EnterpriseReportingPrivateEnqueueRecordFunctionTest,
       InvalidRecordWithMissingTimestampReturnsError) {
  function_->SetProfileIsAffiliatedForTesting(true);
  api::enterprise_reporting_private::EnqueueRecordRequest
      enqueue_record_request;
  // Clear timestamp from test record and set up serialized record
  auto record = GetTestRecord();
  record.clear_timestamp_us();
  serialized_record_data_.clear();
  serialized_record_data_.resize(record.ByteSizeLong());
  ASSERT_TRUE(record.SerializeToArray(serialized_record_data_.data(),
                                      serialized_record_data_.size()));
  enqueue_record_request.record_data = serialized_record_data_;
  enqueue_record_request.priority = ::reporting::Priority::BACKGROUND_BATCH;
  enqueue_record_request.event_type =
      api::enterprise_reporting_private::EventType::kUser;

  base::Value::List params;
  params.Append(enqueue_record_request.ToValue());

  // Set up invalid DM token
  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidToken(kTestDMTokenValue));

  api_test_utils::RunFunction(
      function_.get(), std::move(params),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      extensions::api_test_utils::FunctionMode::kNone);

  EXPECT_EQ(function_->GetError(),
            EnterpriseReportingPrivateEnqueueRecordFunction::
                kErrorInvalidEnqueueRecordRequest);

  VerifyNoRecordsEnqueued();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace {

constexpr char kFakeUserId[] = "fake user id";

enterprise_reporting_private::UserContext GetFakeUserContext() {
  enterprise_reporting_private::UserContext user_context;
  user_context.user_id = kFakeUserId;
  return user_context;
}

std::unique_ptr<KeyedService> BuildMockAggregator(
    content::BrowserContext* context) {
  return std::make_unique<
      testing::StrictMock<device_signals::MockSignalsAggregator>>();
}

}  // namespace

// Base test class for APIs that require a UserContext parameter and which will
// make use of the SignalsAggregator to retrieve device signals.
class UserContextGatedTest : public ExtensionApiUnittest {
 protected:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    auto* factory = enterprise_signals::SignalsAggregatorFactory::GetInstance();
    mock_aggregator_ = static_cast<device_signals::MockSignalsAggregator*>(
        factory->SetTestingFactoryAndUse(
            browser()->profile(), base::BindRepeating(&BuildMockAggregator)));
  }

  void SetFakeResponse(
      const device_signals::SignalsAggregationResponse& response) {
    EXPECT_CALL(*mock_aggregator_, GetSignalsForUser(_, _, _))
        .WillOnce(
            Invoke([&](const device_signals::UserContext& user_context,
                       const device_signals::SignalsAggregationRequest& request,
                       device_signals::SignalsAggregator::GetSignalsCallback
                           callback) {
              EXPECT_EQ(user_context.user_id, kFakeUserId);
              EXPECT_EQ(request.signal_names.size(), 1U);
              std::move(callback).Run(response);
            }));
  }

  virtual void SetFeatureFlag() {
    scoped_features_.InitAndEnableFeature(
        enterprise_signals::features::kNewEvSignalsEnabled);
  }

  raw_ptr<device_signals::MockSignalsAggregator, DanglingUntriaged>
      mock_aggregator_;
  base::test::ScopedFeatureList scoped_features_;
  base::HistogramTester histogram_tester_;
};

// Tests for API enterprise.reportingPrivate.getFileSystemInfo
class EnterpriseReportingPrivateGetFileSystemInfoTest
    : public UserContextGatedTest {
 protected:
  void SetUp() override {
    UserContextGatedTest::SetUp();

    SetFeatureFlag();

    function_ = base::MakeRefCounted<
        EnterpriseReportingPrivateGetFileSystemInfoFunction>();
  }

  device_signals::SignalName signal_name() {
    return device_signals::SignalName::kFileSystemInfo;
  }

  enterprise_reporting_private::GetFileSystemInfoOptions
  GetFakeFileSystemOptionsParam() const {
    enterprise_reporting_private::GetFileSystemInfoOptions api_param;
    api_param.path = "some file path";
    api_param.compute_sha256 = true;
    return api_param;
  }

  std::string GetFakeRequest() const {
    enterprise_reporting_private::GetFileSystemInfoRequest request;
    request.user_context = GetFakeUserContext();
    request.options.push_back(GetFakeFileSystemOptionsParam());
    base::Value::List params;
    params.Append(request.ToValue());
    std::string json_value;
    base::JSONWriter::Write(params, &json_value);
    return json_value;
  }

  scoped_refptr<extensions::EnterpriseReportingPrivateGetFileSystemInfoFunction>
      function_;
};

TEST_F(EnterpriseReportingPrivateGetFileSystemInfoTest, Success) {
  device_signals::FileSystemItem fake_file_item;
  fake_file_item.file_path = base::FilePath();
  fake_file_item.presence = device_signals::PresenceValue::kFound;
  fake_file_item.sha256_hash = "some hashed value";

  device_signals::FileSystemInfoResponse signal_response;
  signal_response.file_system_items.push_back(fake_file_item);

  device_signals::SignalsAggregationResponse expected_response;
  expected_response.file_system_info_response = signal_response;

  SetFakeResponse(expected_response);

  auto response = api_test_utils::RunFunctionAndReturnSingleResult(
      function_.get(), GetFakeRequest(), profile());

  EXPECT_EQ(function_->GetError(), kNoError);

  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_list());
  const base::Value::List& list_value = response->GetList();
  ASSERT_EQ(list_value.size(), signal_response.file_system_items.size());

  const base::Value& file_system_value = list_value.front();
  ASSERT_TRUE(file_system_value.is_dict());
  auto parsed_file_system_signal =
      enterprise_reporting_private::GetFileSystemInfoResponse::FromValue(
          file_system_value.GetDict());
  ASSERT_TRUE(parsed_file_system_signal);
  EXPECT_EQ(parsed_file_system_signal->path,
            fake_file_item.file_path.AsUTF8Unsafe());
  EXPECT_EQ(parsed_file_system_signal->presence,
            enterprise_reporting_private::PresenceValue::kFound);
  EXPECT_EQ(*parsed_file_system_signal->sha256_hash, "c29tZSBoYXNoZWQgdmFsdWU");

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Request.FileSystemInfo.Items", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.FileSystemInfo.Delta", 0, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Success", signal_name(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Success.FileSystemInfo.Items",
      /*number_of_items=*/1,
      /*number_of_occurrences=*/1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success.FileSystemInfo.Latency", 1);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure.FileSystemInfo.Latency", 0);
}

TEST_F(EnterpriseReportingPrivateGetFileSystemInfoTest, TopLevelError) {
  device_signals::SignalCollectionError expected_error =
      device_signals::SignalCollectionError::kConsentRequired;

  device_signals::SignalsAggregationResponse expected_response;
  expected_response.top_level_error = expected_error;
  SetFakeResponse(expected_response);

  auto error = api_test_utils::RunFunctionAndReturnError(
      function_.get(), GetFakeRequest(), profile());

  EXPECT_EQ(error, function_->GetError());
  EXPECT_EQ(error, device_signals::ErrorToString(expected_error));

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Request.FileSystemInfo.Items", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure", signal_name(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure.FileSystemInfo."
      "TopLevelError",
      /*error=*/expected_error,
      /*number_of_occurrences=*/1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure.FileSystemInfo.Latency", 1);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success.FileSystemInfo.Latency", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.FileSystemInfo.Delta", 0);
}

TEST_F(EnterpriseReportingPrivateGetFileSystemInfoTest, CollectionError) {
  device_signals::SignalCollectionError expected_error =
      device_signals::SignalCollectionError::kMissingSystemService;

  device_signals::FileSystemInfoResponse signal_response;
  signal_response.collection_error = expected_error;

  device_signals::SignalsAggregationResponse expected_response;
  expected_response.file_system_info_response = signal_response;
  SetFakeResponse(expected_response);

  auto error = api_test_utils::RunFunctionAndReturnError(
      function_.get(), GetFakeRequest(), profile());

  EXPECT_EQ(error, function_->GetError());
  EXPECT_EQ(error, device_signals::ErrorToString(expected_error));

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Request.FileSystemInfo.Items", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure", signal_name(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure.FileSystemInfo."
      "CollectionLevelError",
      /*error=*/expected_error,
      /*number_of_occurrences=*/1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure.FileSystemInfo.Latency", 1);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success.FileSystemInfo.Latency", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.FileSystemInfo.Delta", 0);
}

class EnterpriseReportingPrivateGetFileSystemInfoDisabledTest
    : public EnterpriseReportingPrivateGetFileSystemInfoTest {
 protected:
  // Overwrite this function to disable the feature flag for tests using this
  // specific fixture.
  void SetFeatureFlag() override {
    scoped_features_.InitAndEnableFeatureWithParameters(
        enterprise_signals::features::kNewEvSignalsEnabled,
        {{"DisableFileSystemInfo", "true"}});
  }
};

TEST_F(EnterpriseReportingPrivateGetFileSystemInfoDisabledTest,
       FlagDisabled_Test) {
  auto error = api_test_utils::RunFunctionAndReturnError(
      function_.get(), GetFakeRequest(), profile());
  EXPECT_EQ(error, function_->GetError());
  EXPECT_EQ(error, device_signals::ErrorToString(
                       device_signals::SignalCollectionError::kUnsupported));
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// Tests for API enterprise.reportingPrivate.getSettings
class EnterpriseReportingPrivateGetSettingsTest : public UserContextGatedTest {
 protected:
  void SetUp() override {
    UserContextGatedTest::SetUp();

    SetFeatureFlag();

    function_ =
        base::MakeRefCounted<EnterpriseReportingPrivateGetSettingsFunction>();
  }

  device_signals::SignalName signal_name() {
    return device_signals::SignalName::kSystemSettings;
  }

  enterprise_reporting_private::GetSettingsOptions GetFakeSettingsOptionsParam()
      const {
    enterprise_reporting_private::GetSettingsOptions api_param;
    api_param.path = "some path";
    api_param.key = "some key";
    api_param.get_value = true;

    api_param.hive =
        enterprise_reporting_private::RegistryHive::kHkeyCurrentUser;

    return api_param;
  }

  std::string GetFakeRequest() const {
    enterprise_reporting_private::GetSettingsRequest request;
    request.user_context = GetFakeUserContext();
    request.options.push_back(GetFakeSettingsOptionsParam());
    base::Value::List params;
    params.Append(request.ToValue());
    std::string json_value;
    base::JSONWriter::Write(params, &json_value);
    return json_value;
  }

  scoped_refptr<extensions::EnterpriseReportingPrivateGetSettingsFunction>
      function_;
};

TEST_F(EnterpriseReportingPrivateGetSettingsTest, Success) {
  device_signals::SettingsItem fake_settings_item;
  fake_settings_item.path = "fake path";
  fake_settings_item.key = "fake key";
  fake_settings_item.presence = device_signals::PresenceValue::kFound;
  std::string setting_json_value = "123";
  fake_settings_item.setting_json_value = setting_json_value;
  fake_settings_item.hive = device_signals::RegistryHive::kHkeyCurrentUser;

  device_signals::SettingsResponse signal_response;
  signal_response.settings_items.push_back(fake_settings_item);

  device_signals::SignalsAggregationResponse expected_response;
  expected_response.settings_response = signal_response;

  SetFakeResponse(expected_response);

  auto response = api_test_utils::RunFunctionAndReturnSingleResult(
      function_.get(), GetFakeRequest(), profile());

  EXPECT_EQ(function_->GetError(), kNoError);

  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_list());
  const base::Value::List& list_value = response->GetList();
  ASSERT_EQ(list_value.size(), signal_response.settings_items.size());

  const base::Value& settings_value = list_value.front();
  ASSERT_TRUE(settings_value.is_dict());
  auto parsed_settings_signal =
      enterprise_reporting_private::GetSettingsResponse::FromValue(
          settings_value.GetDict());
  ASSERT_TRUE(parsed_settings_signal);
  EXPECT_EQ(parsed_settings_signal->path, fake_settings_item.path);
  EXPECT_EQ(parsed_settings_signal->presence,
            enterprise_reporting_private::PresenceValue::kFound);
  ASSERT_TRUE(parsed_settings_signal->value);
  EXPECT_EQ(parsed_settings_signal->value.value(), setting_json_value);

  ASSERT_NE(parsed_settings_signal->hive,
            enterprise_reporting_private::RegistryHive::kNone);
  EXPECT_EQ(parsed_settings_signal->hive,
            enterprise_reporting_private::RegistryHive::kHkeyCurrentUser);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Request.SystemSettings.Items", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.SystemSettings.Delta", 0, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Success", signal_name(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Success.SystemSettings.Items",
      /*number_of_items=*/1,
      /*number_of_occurrences=*/1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success.SystemSettings.Latency", 1);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure.SystemSettings.Latency", 0);
}

TEST_F(EnterpriseReportingPrivateGetSettingsTest, TopLevelError) {
  device_signals::SignalCollectionError expected_error =
      device_signals::SignalCollectionError::kConsentRequired;

  device_signals::SignalsAggregationResponse expected_response;
  expected_response.top_level_error = expected_error;
  SetFakeResponse(expected_response);

  auto error = api_test_utils::RunFunctionAndReturnError(
      function_.get(), GetFakeRequest(), profile());

  EXPECT_EQ(error, function_->GetError());
  EXPECT_EQ(error, device_signals::ErrorToString(expected_error));

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Request.SystemSettings.Items", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure", signal_name(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure.SystemSettings."
      "TopLevelError",
      /*error=*/expected_error,
      /*number_of_occurrences=*/1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure.SystemSettings.Latency", 1);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success.SystemSettings.Latency", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.SystemSettings.Delta", 0);
}

TEST_F(EnterpriseReportingPrivateGetSettingsTest, CollectionError) {
  device_signals::SignalCollectionError expected_error =
      device_signals::SignalCollectionError::kMissingSystemService;

  device_signals::SettingsResponse signal_response;
  signal_response.collection_error = expected_error;

  device_signals::SignalsAggregationResponse expected_response;
  expected_response.settings_response = signal_response;
  SetFakeResponse(expected_response);

  auto error = api_test_utils::RunFunctionAndReturnError(
      function_.get(), GetFakeRequest(), profile());

  EXPECT_EQ(error, function_->GetError());
  EXPECT_EQ(error, device_signals::ErrorToString(expected_error));

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Request.SystemSettings.Items", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure", signal_name(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure.SystemSettings."
      "CollectionLevelError",
      /*error=*/expected_error,
      /*number_of_occurrences=*/1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure.SystemSettings.Latency", 1);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success.SystemSettings.Latency", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.SystemSettings.Delta", 0);
}

class EnterpriseReportingPrivateGetSettingsDisabledTest
    : public EnterpriseReportingPrivateGetSettingsTest {
 protected:
  // Overwrite this function to disable the feature flag for tests using this
  // specific fixture.
  void SetFeatureFlag() override {
    scoped_features_.InitAndEnableFeatureWithParameters(
        enterprise_signals::features::kNewEvSignalsEnabled,
        {{"DisableSettings", "true"}});
  }
};

TEST_F(EnterpriseReportingPrivateGetSettingsDisabledTest, FlagDisabled_Test) {
  auto error = api_test_utils::RunFunctionAndReturnError(
      function_.get(), GetFakeRequest(), profile());
  EXPECT_EQ(error, function_->GetError());
  EXPECT_EQ(error, device_signals::ErrorToString(
                       device_signals::SignalCollectionError::kUnsupported));
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)

std::string GetFakeUserContextJsonParams() {
  auto user_context = GetFakeUserContext();
  base::Value::List params;
  params.Append(user_context.ToValue());
  std::string json_value;
  base::JSONWriter::Write(params, &json_value);
  return json_value;
}

// Tests for API enterprise.reportingPrivate.getAvInfo
class EnterpriseReportingPrivateGetAvInfoTest : public UserContextGatedTest {
 protected:
  void SetUp() override {
    UserContextGatedTest::SetUp();

    SetFeatureFlag();

    function_ =
        base::MakeRefCounted<EnterpriseReportingPrivateGetAvInfoFunction>();
  }

  device_signals::SignalName signal_name() {
    return device_signals::SignalName::kAntiVirus;
  }

  scoped_refptr<extensions::EnterpriseReportingPrivateGetAvInfoFunction>
      function_;
};

TEST_F(EnterpriseReportingPrivateGetAvInfoTest, Success) {
  device_signals::AvProduct fake_av_product;
  fake_av_product.display_name = "Fake display name";
  fake_av_product.state = device_signals::AvProductState::kOff;
  fake_av_product.product_id = "fake product id";

  device_signals::AntiVirusSignalResponse av_response;
  av_response.av_products.push_back(fake_av_product);

  device_signals::SignalsAggregationResponse expected_response;
  expected_response.av_signal_response = av_response;

  SetFakeResponse(expected_response);

  auto response = api_test_utils::RunFunctionAndReturnSingleResult(
      function_.get(), GetFakeUserContextJsonParams(), profile());

  EXPECT_EQ(function_->GetError(), kNoError);

  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_list());
  const base::Value::List& list_value = response->GetList();
  ASSERT_EQ(list_value.size(), av_response.av_products.size());

  const base::Value& av_value = list_value.front();
  ASSERT_TRUE(av_value.is_dict());
  auto parsed_av_signal =
      enterprise_reporting_private::AntiVirusSignal::FromValue(
          av_value.GetDict());
  ASSERT_TRUE(parsed_av_signal);
  EXPECT_EQ(parsed_av_signal->display_name, fake_av_product.display_name);
  EXPECT_EQ(parsed_av_signal->state,
            enterprise_reporting_private::AntiVirusProductState::kOff);
  EXPECT_EQ(parsed_av_signal->product_id, fake_av_product.product_id);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Success", signal_name(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Success.AntiVirus.Items",
      /*number_of_items=*/1,
      /*number_of_occurrences=*/1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success.AntiVirus.Latency", 1);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure.AntiVirus.Latency", 0);
}

TEST_F(EnterpriseReportingPrivateGetAvInfoTest, TopLevelError) {
  device_signals::SignalCollectionError expected_error =
      device_signals::SignalCollectionError::kConsentRequired;

  device_signals::SignalsAggregationResponse expected_response;
  expected_response.top_level_error = expected_error;
  SetFakeResponse(expected_response);

  auto error = api_test_utils::RunFunctionAndReturnError(
      function_.get(), GetFakeUserContextJsonParams(), profile());

  EXPECT_EQ(error, function_->GetError());
  EXPECT_EQ(error, device_signals::ErrorToString(expected_error));

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure", signal_name(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure.AntiVirus.TopLevelError",
      /*error=*/expected_error,
      /*number_of_occurrences=*/1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure.AntiVirus.Latency", 1);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success.AntiVirus.Latency", 0);
}

TEST_F(EnterpriseReportingPrivateGetAvInfoTest, CollectionError) {
  device_signals::SignalCollectionError expected_error =
      device_signals::SignalCollectionError::kMissingSystemService;

  device_signals::AntiVirusSignalResponse av_response;
  av_response.collection_error = expected_error;

  device_signals::SignalsAggregationResponse expected_response;
  expected_response.av_signal_response = av_response;
  SetFakeResponse(expected_response);

  auto error = api_test_utils::RunFunctionAndReturnError(
      function_.get(), GetFakeUserContextJsonParams(), profile());

  EXPECT_EQ(error, function_->GetError());
  EXPECT_EQ(error, device_signals::ErrorToString(expected_error));

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure", signal_name(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure.AntiVirus."
      "CollectionLevelError",
      /*error=*/expected_error,
      /*number_of_occurrences=*/1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure.AntiVirus.Latency", 1);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success.AntiVirus.Latency", 0);
}

class EnterpriseReportingPrivateGetAvInfoDisabledTest
    : public EnterpriseReportingPrivateGetAvInfoTest {
 protected:
  // Overwrite this function to disable the feature flag for tests using this
  // specific fixture.
  void SetFeatureFlag() override {
    scoped_features_.InitAndEnableFeatureWithParameters(
        enterprise_signals::features::kNewEvSignalsEnabled,
        {{"DisableAntiVirus", "true"}});
  }
};

TEST_F(EnterpriseReportingPrivateGetAvInfoDisabledTest, FlagDisabled_Test) {
  auto error = api_test_utils::RunFunctionAndReturnError(
      function_.get(), GetFakeUserContextJsonParams(), profile());
  EXPECT_EQ(error, function_->GetError());
  EXPECT_EQ(error, device_signals::ErrorToString(
                       device_signals::SignalCollectionError::kUnsupported));
}

// Tests for API enterprise.reportingPrivate.getHotfixes
class EnterpriseReportingPrivateGetHotfixesTest : public UserContextGatedTest {
 protected:
  void SetUp() override {
    UserContextGatedTest::SetUp();

    SetFeatureFlag();

    function_ =
        base::MakeRefCounted<EnterpriseReportingPrivateGetHotfixesFunction>();
  }

  device_signals::SignalName signal_name() {
    return device_signals::SignalName::kHotfixes;
  }

  scoped_refptr<extensions::EnterpriseReportingPrivateGetHotfixesFunction>
      function_;
};

TEST_F(EnterpriseReportingPrivateGetHotfixesTest, Success) {
  static constexpr char kFakeHotfixId[] = "hotfix id";
  device_signals::HotfixSignalResponse hotfix_response;
  hotfix_response.hotfixes.push_back({kFakeHotfixId});

  device_signals::SignalsAggregationResponse expected_response;
  expected_response.hotfix_signal_response = hotfix_response;

  SetFakeResponse(expected_response);

  auto response = api_test_utils::RunFunctionAndReturnSingleResult(
      function_.get(), GetFakeUserContextJsonParams(), profile());

  EXPECT_EQ(function_->GetError(), kNoError);

  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_list());
  const base::Value::List& list_value = response->GetList();
  ASSERT_EQ(list_value.size(), hotfix_response.hotfixes.size());

  const base::Value& hotfix_value = list_value.front();
  ASSERT_TRUE(hotfix_value.is_dict());
  auto parsed_hotfix = enterprise_reporting_private::HotfixSignal::FromValue(
      hotfix_value.GetDict());
  ASSERT_TRUE(parsed_hotfix);
  EXPECT_EQ(parsed_hotfix->hotfix_id, kFakeHotfixId);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Success", signal_name(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Success.Hotfixes.Items",
      /*number_of_items=*/1,
      /*number_of_occurrences=*/1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success.Hotfixes.Latency", 1);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure.Hotfixes.Latency", 0);
}

TEST_F(EnterpriseReportingPrivateGetHotfixesTest, TopLevelError) {
  device_signals::SignalCollectionError expected_error =
      device_signals::SignalCollectionError::kConsentRequired;

  device_signals::SignalsAggregationResponse expected_response;
  expected_response.top_level_error = expected_error;
  SetFakeResponse(expected_response);

  auto error = api_test_utils::RunFunctionAndReturnError(
      function_.get(), GetFakeUserContextJsonParams(), profile());

  EXPECT_EQ(error, function_->GetError());
  EXPECT_EQ(error, device_signals::ErrorToString(expected_error));

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure", signal_name(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure.Hotfixes.TopLevelError",
      /*error=*/expected_error,
      /*number_of_occurrences=*/1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure.Hotfixes.Latency", 1);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success.Hotfixes.Latency", 0);
}

TEST_F(EnterpriseReportingPrivateGetHotfixesTest, CollectionError) {
  device_signals::SignalCollectionError expected_error =
      device_signals::SignalCollectionError::kMissingSystemService;

  device_signals::HotfixSignalResponse hotfix_response;
  hotfix_response.collection_error = expected_error;

  device_signals::SignalsAggregationResponse expected_response;
  expected_response.hotfix_signal_response = hotfix_response;
  SetFakeResponse(expected_response);

  auto error = api_test_utils::RunFunctionAndReturnError(
      function_.get(), GetFakeUserContextJsonParams(), profile());

  EXPECT_EQ(error, function_->GetError());
  EXPECT_EQ(error, device_signals::ErrorToString(expected_error));

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure", signal_name(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Failure.Hotfixes."
      "CollectionLevelError",
      /*error=*/expected_error,
      /*number_of_occurrences=*/1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Failure.Hotfixes.Latency", 1);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Collection.Success.Hotfixes.Latency", 0);
}

class EnterpriseReportingPrivateGetHotfixesInfoDisabledTest
    : public EnterpriseReportingPrivateGetHotfixesTest {
 protected:
  // Overwrite this function to disable the feature flag for tests using this
  // specific fixture.
  void SetFeatureFlag() override {
    scoped_features_.InitAndEnableFeatureWithParameters(
        enterprise_signals::features::kNewEvSignalsEnabled,
        {{"DisableHotfix", "true"}});
  }
};

TEST_F(EnterpriseReportingPrivateGetHotfixesInfoDisabledTest,
       FlagDisabled_Test) {
  auto error = api_test_utils::RunFunctionAndReturnError(
      function_.get(), GetFakeUserContextJsonParams(), profile());
  EXPECT_EQ(error, function_->GetError());
  EXPECT_EQ(error, device_signals::ErrorToString(
                       device_signals::SignalCollectionError::kUnsupported));
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace extensions
