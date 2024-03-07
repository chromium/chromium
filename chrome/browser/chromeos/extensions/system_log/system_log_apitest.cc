// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chrome/browser/feedback/system_logs/log_sources/device_event_log_source.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/device_event_log/device_event_log.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

namespace {

constexpr char kApiExtensionRelativePath[] = "extensions/api_test/system_log";
constexpr char kExtensionPemRelativePath[] =
    "extensions/api_test/system_log.pem";
// ID associated with the .pem.
constexpr char kExtensionId[] = "ghbglelacokpaehlgjbgdfmmggnihdcf";

constexpr char kDeviceEventLogEntry[] = "device_event_log";

const char kManagedAccountId[] = "managed-guest-account@test";

}  // namespace

// Verifies the systemLog API logs on the sign-in screen.
class SystemLogSigninScreenApitest
    : public MixinBasedExtensionApiTest,
      public ::testing::WithParamInterface<std::string> {
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

// Logs to system logs and DEBUG device event logs.
IN_PROC_BROWSER_TEST_P(SystemLogSigninScreenApitest, AddLogFromSignInScreen) {
  const std::string test_name = GetParam();
  SetCustomArg(test_name);

  ResultCatcher catcher;
  ForceInstallExtension();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  std::string produced_debug_logs = device_event_log::GetAsString(
      device_event_log::NEWEST_FIRST, /*format=*/"level",
      /*types=*/"extensions",
      /*max_level=*/device_event_log::LOG_LEVEL_DEBUG, /*max_events=*/1);
  std::string expected_logs = base::StringPrintf(
      "DEBUG: [%s][signin]: Test log message\n", kExtensionId);

  ASSERT_EQ(expected_logs, produced_debug_logs);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemLogSigninScreenApitest,
    /*test_name=*/testing::Values("AddLogWithCallback", "AddLogWithPromise"));

// Verifies the systemLog API logs in user sessions.
class SystemLogUserSessionApitest
    : public MixinBasedExtensionApiTest,
      public ::testing::WithParamInterface<std::string> {
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

// Logs go to device event logs with an EVENT log level and logs are added to
// the feedback report fetched data.
IN_PROC_BROWSER_TEST_P(SystemLogUserSessionApitest, AddLogFromUserSession) {
  const std::string test_name = GetParam();
  SetCustomArg(test_name);

  ResultCatcher catcher;
  ForceInstallExtension();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  std::string produced_event_logs = device_event_log::GetAsString(
      device_event_log::NEWEST_FIRST, /*format=*/"level",
      /*types=*/"extensions",
      /*max_level=*/device_event_log::LOG_LEVEL_EVENT, /*max_events=*/1);
  std::string expected_logs =
      base::StringPrintf("EVENT: [%s]: Test log message\n", kExtensionId);
  ASSERT_EQ(expected_logs, produced_event_logs);

  // Verify that logs are added to feedback report strings.
  auto log_source = std::make_unique<system_logs::DeviceEventLogSource>();
  base::test::TestFuture<std::unique_ptr<system_logs::SystemLogsResponse>>
      future;
  log_source->Fetch(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  system_logs::SystemLogsResponse* response = future.Get().get();
  const auto device_event_log_iter = response->find(kDeviceEventLogEntry);
  EXPECT_NE(device_event_log_iter, response->end());

  std::string expected_feedback_log =
      base::StringPrintf("[%s]: Test log message\n", kExtensionId);
  EXPECT_THAT(device_event_log_iter->second,
              ::testing::HasSubstr(expected_feedback_log));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemLogUserSessionApitest,
    /*test_name=*/testing::Values("AddLogWithCallback", "AddLogWithPromise"));

// Verifies the systemLog API logs in managed guest sessions.
class SystemLogManagedGuestSessionApitest
    : public policy::DevicePolicyCrosBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);
    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    kExtensionId);
  }

  void SetUpDeviceLocalAccountPolicy() {
    enterprise_management::ChromeDeviceSettingsProto& proto(
        device_policy()->payload());
    enterprise_management::DeviceLocalAccountsProto* device_local_accounts =
        proto.mutable_device_local_accounts();
    enterprise_management::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kManagedAccountId);
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);
    device_local_accounts->set_auto_login_id(kManagedAccountId);
    device_local_accounts->set_auto_login_delay(0);
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

  void ForceInstallExtension() {
    base::FilePath test_dir_path =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);

    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
        test_dir_path.AppendASCII(kApiExtensionRelativePath),
        test_dir_path.AppendASCII(kExtensionPemRelativePath),
        ExtensionForceInstallMixin::WaitMode::kLoad));
  }

  Profile* GetActiveUserProfile() {
    const user_manager::User* active_user =
        user_manager::UserManager::Get()->GetActiveUser();
    return Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByUser(active_user));
  }

  void SetTestCustomArg(const std::string custom_arg) {
    config_.Set("customArg", base::Value(custom_arg));
    extensions::TestGetConfigFunction::set_test_config_state(&config_);
  }

 protected:
  base::Value::Dict config_;
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
};

// Logs to system logs and DEBUG device event logs.
IN_PROC_BROWSER_TEST_P(SystemLogManagedGuestSessionApitest,
                       AddLogFromManagedGuestSession) {
  SetUpDeviceLocalAccountPolicy();
  ash::test::WaitForPrimaryUserSessionStart();
  Profile* profile = GetActiveUserProfile();

  policy::UserPolicyBuilder user_policy_builder;
  SetUpUserPolicyBuilderForPublicAccount(&user_policy_builder);

  extension_force_install_mixin_.InitWithEmbeddedPolicyMixin(
      profile, &policy_test_server_mixin_, &user_policy_builder,
      kManagedAccountId, policy::dm_protocol::kChromePublicAccountPolicyType);

  const std::string test_name = GetParam();
  SetTestCustomArg(test_name);
  ResultCatcher catcher;
  ForceInstallExtension();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  std::string produced_debug_logs = device_event_log::GetAsString(
      device_event_log::NEWEST_FIRST, /*format=*/"level",
      /*types=*/"extensions",
      /*max_level=*/device_event_log::LOG_LEVEL_DEBUG, /*max_events=*/1);
  std::string expected_logs =
      base::StringPrintf("DEBUG: [%s]: Test log message\n", kExtensionId);

  ASSERT_EQ(expected_logs, produced_debug_logs);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemLogManagedGuestSessionApitest,
    /*test_name=*/testing::Values("AddLogWithCallback", "AddLogWithPromise"));

}  // namespace extensions
