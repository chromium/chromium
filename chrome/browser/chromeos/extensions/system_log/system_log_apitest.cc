// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chrome/browser/feedback/system_logs/log_sources/device_event_log_source.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/device_event_log/device_event_log.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#endif

namespace extensions {

namespace {

constexpr char kApiExtensionRelativePath[] = "extensions/api_test/system_log";
constexpr char kExtensionPemRelativePath[] =
    "extensions/api_test/system_log.pem";
// ID associated with the .pem.
constexpr char kExtensionId[] = "ghbglelacokpaehlgjbgdfmmggnihdcf";

constexpr char kDeviceEventLogEntry[] = "device_event_log";

// Test names for when chrome.systemLog is available to the extension (if policy
// installed) and when chrome.systemLog is undefined (user installed).
constexpr char kSystemLogAvailableTestName[] = "SystemLogAvailable";
constexpr char kSystemLogUndefinedTestName[] = "SystemLogUndefined";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kManagedAccountId[] = "managed-guest-account@test";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void VerifyDeviceEventLogLevel(const std::string& log_level,
                               bool on_signin_screen = false) {
  const std::string produced_logs = device_event_log::GetAsString(
      device_event_log::NEWEST_FIRST, /*format=*/"level",
      /*types=*/"extensions",
      /*max_level=*/device_event_log::LOG_LEVEL_DEBUG, /*max_events=*/1);

  const std::string expected_logs =
      base::StringPrintf("%s: [%s]%s: Test log message\n", log_level.c_str(),
                         kExtensionId, on_signin_screen ? "[signin]" : "");

  EXPECT_EQ(expected_logs, produced_logs);
}

bool AreLogsForwardedToFeedbackReport(bool on_signin_screen = false) {
  auto log_source = std::make_unique<system_logs::DeviceEventLogSource>();
  base::test::TestFuture<std::unique_ptr<system_logs::SystemLogsResponse>>
      future;
  log_source->Fetch(future.GetCallback());
  EXPECT_TRUE(future.Wait());

  const std::string expected_feedback_log =
      base::StringPrintf("[%s]%s: Test log message\n", kExtensionId,
                         on_signin_screen ? "[signin]" : "");

  system_logs::SystemLogsResponse* response = future.Get().get();
  const auto device_event_log_iter = response->find(kDeviceEventLogEntry);

  return device_event_log_iter != response->end() &&
         device_event_log_iter->second.find(expected_feedback_log) !=
             std::string::npos;
}

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Verifies the systemLog API logs on the sign-in screen.
class SystemLogSigninScreenApitest
    : public MixinBasedExtensionApiTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);
    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    kExtensionId);

    MixinBasedExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    MixinBasedExtensionApiTest::SetUpOnMainThread();
    extension_force_install_mixin_.InitWithDeviceStateMixin(
        GetOriginalSigninProfile(), &device_state_mixin_);
  }

  void SetSystemLogPolicy(bool system_logging_enabled) {
    device_state_mixin_.RequestDevicePolicyUpdate()
        ->policy_payload()
        ->mutable_deviceextensionssystemlogenabled()
        ->set_value(system_logging_enabled);
  }

  void ForceInstallExtension() {
    base::FilePath test_dir_path =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);

    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
        test_dir_path.AppendASCII(kApiExtensionRelativePath),
        test_dir_path.AppendASCII(kExtensionPemRelativePath),
        ExtensionForceInstallMixin::WaitMode::kLoad));
  }

  Profile* GetOriginalSigninProfile() {
    return Profile::FromBrowserContext(
               ash::BrowserContextHelper::Get()->GetSigninBrowserContext())
        ->GetOriginalProfile();
  }

 private:
  ash::DeviceStateMixin device_state_mixin_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::LoginManagerMixin login_manager_mixin_{&mixin_host_};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
};

// Logs EVENT or DEBUG extension logs depending on the
// DeviceExtensionsSystemLogEnabled policy.
IN_PROC_BROWSER_TEST_P(SystemLogSigninScreenApitest, AddLogFromSignInScreen) {
  const bool system_logging_enabled = GetParam();
  SetSystemLogPolicy(system_logging_enabled);

  SetCustomArg(kSystemLogAvailableTestName);
  ResultCatcher catcher;

  ForceInstallExtension();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  const std::string log_level = system_logging_enabled ? "DEBUG" : "EVENT";
  VerifyDeviceEventLogLevel(log_level, /*on_signin_screen=*/true);

  // Logs are forwarded to the feedback report if they are added to the device
  // event log with an EVENT log level. Otherwise the logs will be added to the
  // feedback report via the system log file.
  EXPECT_EQ(AreLogsForwardedToFeedbackReport(/*on_signin_screen=*/true),
            !system_logging_enabled);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SystemLogSigninScreenApitest,
                         /*system_logging_enabled=*/testing::Bool());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Verifies the systemLog API logs in user sessions.
class SystemLogUserSessionApitest : public MixinBasedExtensionApiTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedExtensionApiTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    kExtensionId);
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedExtensionApiTest::SetUpInProcessBrowserTestFixture();

    mock_policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    mock_policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &mock_policy_provider_);
  }

  void SetUpOnMainThread() override {
    extension_force_install_mixin_.InitWithMockPolicyProvider(
        profile(), &mock_policy_provider_);

    MixinBasedExtensionApiTest::SetUpOnMainThread();
  }

  void SetSystemLogPolicy(bool system_logging_enabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    policy::PolicyMap policy_map =
        mock_policy_provider_.policies()
            .Get(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                         /*component_id=*/std::string()))
            .Clone();
    policy_map.Set(policy::key::kDeviceExtensionsSystemLogEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
    mock_policy_provider_.UpdateChromePolicy(policy_map);
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
    auto params = chromeos::BrowserInitParams::GetForTests()->Clone();
    params->session_type = crosapi::mojom::SessionType::kRegularSession;
    params->device_settings = crosapi::mojom::DeviceSettings::New();
    params->device_settings->device_extensions_system_log_enabled =
        system_logging_enabled
            ? crosapi::mojom::DeviceSettings::OptionalBool::kTrue
            : crosapi::mojom::DeviceSettings::OptionalBool::kFalse;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
#endif
  }

  void ForceInstallExtension() {
    base::FilePath test_dir_path =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);

    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
        test_dir_path.AppendASCII(kApiExtensionRelativePath),
        test_dir_path.AppendASCII(kExtensionPemRelativePath),
        ExtensionForceInstallMixin::WaitMode::kLoad));
  }

 private:
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      mock_policy_provider_;
};

// Logs EVENT extension logs irrespective of the
// DeviceExtensionsSystemLogEnabled policy.
IN_PROC_BROWSER_TEST_P(SystemLogUserSessionApitest, AddLogFromUserSession) {
  const bool system_logging_enabled = GetParam();
  SetSystemLogPolicy(system_logging_enabled);

  SetCustomArg(kSystemLogAvailableTestName);
  ResultCatcher catcher;

  ForceInstallExtension();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Logs are always added to the device event log buffer with the EVENT level.
  VerifyDeviceEventLogLevel("EVENT");

  // Logs are always forwarded to the feedback report via the device event log
  // buffer.
  EXPECT_TRUE(AreLogsForwardedToFeedbackReport());
}

IN_PROC_BROWSER_TEST_P(SystemLogUserSessionApitest,
                       DeniesNonPolicyInstalledExtensions) {
  const bool system_logging_enabled = GetParam();
  SetSystemLogPolicy(system_logging_enabled);

  SetCustomArg(kSystemLogUndefinedTestName);
  ResultCatcher catcher;

  // Add user installed extension.
  extensions::ChromeTestExtensionLoader loader(profile());
  base::FilePath extension_path =
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kApiExtensionRelativePath);
  loader.set_location(extensions::mojom::ManifestLocation::kInternal);
  loader.set_pack_extension(true);
  loader.set_ignore_manifest_warnings(true);
  loader.LoadExtension(extension_path);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

INSTANTIATE_TEST_SUITE_P(All,
                         SystemLogUserSessionApitest,
                         /*system_logging_enabled=*/testing::Bool());

// Verifies the systemLog API logs in managed guest sessions.
#if BUILDFLAG(IS_CHROMEOS_ASH)
class SystemLogManagedGuestSessionApitest
    : public policy::DevicePolicyCrosBrowserTest,
      public ::testing::WithParamInterface<bool> {
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
class SystemLogManagedGuestSessionApitest
    : public MixinBasedExtensionApiTest,
      public ::testing::WithParamInterface<bool> {
#endif

 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
    MixinBasedExtensionApiTest::SetUpCommandLine(command_line);
#endif
    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    kExtensionId);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetSystemLogPolicy(bool system_logging_enabled) {
    SetDevicePolicies(system_logging_enabled);
    ash::test::WaitForPrimaryUserSessionStart();
    Profile* profile = GetActiveUserProfile();

    SetUpUserPolicyBuilderForPublicAccount(&user_policy_builder_);

    extension_force_install_mixin_.InitWithEmbeddedPolicyMixin(
        profile, &policy_test_server_mixin_, &user_policy_builder_,
        kManagedAccountId, policy::dm_protocol::kChromePublicAccountPolicyType);
  }

  void SetDevicePolicies(bool system_logging_enabled) {
    enterprise_management::ChromeDeviceSettingsProto& proto(
        device_policy()->payload());
    // Set Managed Guest Session policy.
    enterprise_management::DeviceLocalAccountsProto* device_local_accounts =
        proto.mutable_device_local_accounts();
    enterprise_management::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kManagedAccountId);
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);
    device_local_accounts->set_auto_login_id(kManagedAccountId);
    device_local_accounts->set_auto_login_delay(0);

    // Set System Log policy.
    proto.mutable_deviceextensionssystemlogenabled()->set_value(
        system_logging_enabled);
    RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  void SetUpUserPolicyBuilderForPublicAccount(
      policy::UserPolicyBuilder* user_policy_builder) {
    enterprise_management::PolicyData& policy_data =
        user_policy_builder->policy_data();
    policy_data.set_public_key_version(1);
    policy_data.set_policy_type(
        policy::dm_protocol::kChromePublicAccountPolicyType);
    policy_data.set_username(kManagedAccountId);
    policy_data.set_settings_entity_id(kManagedAccountId);
    user_policy_builder->SetDefaultSigningKey();
  }

  Profile* GetActiveUserProfile() {
    const user_manager::User* active_user =
        user_manager::UserManager::Get()->GetActiveUser();
    return Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByUser(active_user));
  }
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedExtensionApiTest::SetUpInProcessBrowserTestFixture();

    mock_policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    mock_policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &mock_policy_provider_);
  }

  void SetUpOnMainThread() override {
    extension_force_install_mixin_.InitWithMockPolicyProvider(
        profile(), &mock_policy_provider_);

    MixinBasedExtensionApiTest::SetUpOnMainThread();
  }

  void SetSystemLogPolicy(bool system_logging_enabled) {
    auto params = chromeos::BrowserInitParams::GetForTests()->Clone();
    params->session_type = crosapi::mojom::SessionType::kPublicSession;
    params->device_settings = crosapi::mojom::DeviceSettings::New();
    params->device_settings->device_extensions_system_log_enabled =
        system_logging_enabled
            ? crosapi::mojom::DeviceSettings::OptionalBool::kTrue
            : crosapi::mojom::DeviceSettings::OptionalBool::kFalse;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
  }
#endif

  void ForceInstallExtension() {
    base::FilePath test_dir_path =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);

    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
        test_dir_path.AppendASCII(kApiExtensionRelativePath),
        test_dir_path.AppendASCII(kExtensionPemRelativePath),
        ExtensionForceInstallMixin::WaitMode::kLoad));
  }

  void SetTestCustomArg(const std::string custom_arg) {
    config_.Set("customArg", base::Value(custom_arg));
    extensions::TestGetConfigFunction::set_test_config_state(&config_);
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  policy::UserPolicyBuilder user_policy_builder_;
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      mock_policy_provider_;
#endif
  base::Value::Dict config_;
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
};

// Logs EVENT or DEBUG extension logs depending on the
// DeviceExtensionsSystemLogEnabled policy.
IN_PROC_BROWSER_TEST_P(SystemLogManagedGuestSessionApitest,
                       AddLogFromManagedGuestSession) {
  const bool system_logging_enabled = GetParam();
  SetSystemLogPolicy(system_logging_enabled);

  SetTestCustomArg(kSystemLogAvailableTestName);
  ResultCatcher catcher;

  ForceInstallExtension();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  const std::string log_level = system_logging_enabled ? "DEBUG" : "EVENT";
  VerifyDeviceEventLogLevel(log_level);

  // Logs are forwarded to the feedback report if they are added to the device
  // event log with an EVENT log level. Otherwise the logs will be added to the
  // feedback report via the system log file.
  EXPECT_EQ(AreLogsForwardedToFeedbackReport(), !system_logging_enabled);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SystemLogManagedGuestSessionApitest,
                         /*system_logging_enabled=*/testing::Bool());
}  // namespace extensions
