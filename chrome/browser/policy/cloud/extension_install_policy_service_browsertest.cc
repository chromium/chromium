// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/extension_install_policy_service.h"

#include "base/test/test_future.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_delegate_desktop.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_client_types.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_urls.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Mock;

namespace {
constexpr char kExtensionId1[] = "extension1";
constexpr char kExtensionVersion1[] = "1.0.0.0";
constexpr char kExtensionId2[] = "extension2";
constexpr char kExtensionVersion2[] = "2.0.0.0";
constexpr char kEnrollmentToken[] = "enrollment_token";
constexpr char kMachineClientID[] = "fake_browser_client_id";
constexpr char kUserClientID[] = "fake_user_client_id";
constexpr char kDMToken[] = "fake_dm_token";
constexpr char kTestUser[] = "test@example.com";

ClientStorage::ClientInfo CreateTestClientInfo() {
  ClientStorage::ClientInfo client_info;
  client_info.device_id = kMachineClientID;
  client_info.device_token = kDMToken;
  client_info.allowed_policy_types.insert(
      {dm_protocol::kChromeMachineLevelUserCloudPolicyType,
       dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
       dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType});
  return client_info;
}

}  // namespace

#if !BUILDFLAG(IS_CHROMEOS)
class ExtensionInstallPolicyServiceTest : public PolicyTest {
 public:
  ExtensionInstallPolicyServiceTest() {
    BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.SetEnrollmentToken(kEnrollmentToken);
    storage_.SetClientId(kMachineClientID);
    storage_.EnableStorage(true);
    storage_.SetDMToken(kDMToken);
  }
  ExtensionInstallPolicyServiceTest(const ExtensionInstallPolicyServiceTest&) =
      delete;
  ExtensionInstallPolicyServiceTest& operator=(
      const ExtensionInstallPolicyServiceTest&) = delete;

  void SetUp() override {
    if (!IsExtensionInstallPolicySupportedOnThisVersion()) {
      GTEST_SKIP() << "Extension install policy is not supported on this "
                      "version of Chrome.";
    }
    PolicyTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();

    test_server_ = std::make_unique<policy::EmbeddedPolicyTestServer>();
    test_server_->client_storage()->RegisterClient(CreateTestClientInfo());
    ASSERT_TRUE(test_server_->Start());
    test_server_->policy_storage()->add_managed_user("*");
    test_server_->policy_storage()->set_policy_user(kTestUser);
    test_server_->policy_storage()
        ->signature_provider()
        ->SetUniversalSigningKeys();

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl,
                                    test_server_->GetServiceURL().spec());
    ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableChromeBrowserCloudManagement);
  }
#endif

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();

    policy::BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    connector->ScheduleServiceInitialization(0);

    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_env_->MakePrimaryAccountAvailable(
        kTestUser, signin::ConsentLevel::kSignin);

    policy::UserCloudPolicyManager* policy_manager =
        browser()->profile()->GetUserCloudPolicyManager();
    policy_manager->Connect(
        g_browser_process->local_state(),
        std::make_unique<policy::CloudPolicyClient>(
            g_browser_process->browser_policy_connector()
                ->device_management_service(),
            g_browser_process->shared_url_loader_factory()));

    // Prevent auto policy fetch after register as we don't need to test that.
    policy_manager->core()->client()->RemoveObserver(
        policy_manager->core()->refresh_scheduler());

    ASSERT_NO_FATAL_FAILURE(RegisterUser(policy_manager->core()->client()));
  }

  // Register the user with fake DM Server.
  void RegisterUser(policy::CloudPolicyClient* client) {
    base::test::TestFuture<void> registered_signal;
    policy::MockCloudPolicyClientObserver observer;
    EXPECT_CALL(observer, OnRegistrationStateChanged(_))
        .WillOnce(InvokeWithoutArgs(
            [&registered_signal]() { registered_signal.SetValue(); }));
    client->AddObserver(&observer);

    ASSERT_FALSE(client->is_registered());
    policy::CloudPolicyClient::RegistrationParameters parameters(
        em::DeviceRegisterRequest::BROWSER,
        em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
    client->Register(parameters, kUserClientID, "oauth_token_unused");
    EXPECT_TRUE(registered_signal.Wait());

    Mock::VerifyAndClearExpectations(&observer);

    client->RemoveObserver(&observer);
    EXPECT_TRUE(client->is_registered());

    ClientStorage::ClientInfo user_client_info =
        test_server_->client_storage()->GetClient(kUserClientID);
    user_client_info.allowed_policy_types.insert(
        dm_protocol::kChromeExtensionInstallUserCloudPolicyType);
    test_server_->client_storage()->RegisterClient(user_client_info);
  }

  void SetExtensionInstallPolicy(
      const std::string& extension_id,
      const std::string& extension_version,
      enterprise_management::ExtensionInstallPolicy::Action action,
      const std::vector<enterprise_management::ExtensionInstallPolicy::Reason>&
          reasons,
      bool is_machine_level) {
    enterprise_management::ExtensionInstallPolicies extension_install_policies;
    enterprise_management::ExtensionInstallPolicy* policy =
        extension_install_policies.add_policies();
    policy->set_extension_id(extension_id);
    policy->set_extension_version(extension_version);
    policy->set_action(action);
    for (const auto& allowed_reason : reasons) {
      policy->add_reasons(allowed_reason);
    }
    test_server_->policy_storage()->SetPolicyPayload(
        is_machine_level
            ? dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType
            : dm_protocol::kChromeExtensionInstallUserCloudPolicyType,
        base::StrCat({extension_id, "@", extension_version}),
        extension_install_policies.SerializeAsString());
  }

  void CheckCanInstallExtension(const std::string& extension_id,
                                const std::string& extension_version,
                                bool expected_result) {
    ExtensionInstallPolicyServiceImpl service(browser()->profile());
    base::test::TestFuture<bool> future;
    service.CanInstallExtension(
        ExtensionIdAndVersion(extension_id, extension_version),
        future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_EQ(future.Get(), expected_result);
    service.Shutdown();
  }

  void CheckUserMayInstall(const std::string& extension_id,
                           const std::string& extension_version,
                           bool is_from_webstore,
                           bool expected_result) {
    ExtensionInstallPolicyServiceImpl service(browser()->profile());
    base::test::TestFuture<extensions::ManagementPolicy::Decision> future;
    std::u16string error;
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("Test Extension")
            .SetID(extension_id)
            .SetVersion(extension_version)
            .AddFlags(is_from_webstore ? extensions::Extension::FROM_WEBSTORE
                                       : extensions::Extension::NO_FLAGS)
            .Build();
    service.UserMayInstall(extension.get(), future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_EQ(future.Get().allowed, expected_result);
    service.Shutdown();
  }

 protected:
  std::unique_ptr<EmbeddedPolicyTestServer> test_server_;
  FakeBrowserDMTokenStorage storage_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kEnableExtensionInstallPolicyFetching};
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallPolicyServiceTest,
                       CanInstallExtensionAllowedByDefault) {
  ExtensionInstallPolicyServiceImpl service(browser()->profile());
  base::test::TestFuture<bool> future;
  service.CanInstallExtension(
      ExtensionIdAndVersion(kExtensionId1, kExtensionVersion1),
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
  ASSERT_NO_FATAL_FAILURE(CheckUserMayInstall(kExtensionId1, kExtensionVersion1,
                                              /*is_from_webstore=*/true,
                                              /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckUserMayInstall(kExtensionId1, kExtensionVersion1,
                                              /*is_from_webstore=*/false,
                                              /*expected_result=*/true));
  service.Shutdown();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallPolicyServiceTest,
                       CanInstallExtensionBlockedByMachineLevelPolicy) {
  browser()->profile()->GetPrefs()->SetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled, true);

  SetExtensionInstallPolicy(
      kExtensionId1, kExtensionVersion1,
      enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK,
      {enterprise_management::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY},
      /*is_machine_level=*/true);

  CheckCanInstallExtension(kExtensionId1, kExtensionVersion1,
                           /*expected_result=*/false);
  // Unrelated extension versions are not blocked.
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));

  ASSERT_NO_FATAL_FAILURE(CheckUserMayInstall(kExtensionId1, kExtensionVersion1,
                                              /*is_from_webstore=*/true,
                                              /*expected_result=*/false));
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallPolicyServiceTest,
                       AlwaysAllowNonWebstoreExtensions) {
  browser()->profile()->GetPrefs()->SetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled, true);
  SetExtensionInstallPolicy(
      kExtensionId1, kExtensionVersion1,
      enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK,
      {enterprise_management::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY},
      /*is_machine_level=*/true);
  ASSERT_NO_FATAL_FAILURE(CheckUserMayInstall(kExtensionId1, kExtensionVersion1,
                                              /*is_from_webstore=*/false,
                                              /*expected_result=*/true));
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallPolicyServiceTest,
                       CanInstallExtensionBlockedByUserLevelPolicy) {
  browser()->profile()->GetPrefs()->SetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled, true);

  SetExtensionInstallPolicy(
      kExtensionId1, kExtensionVersion1,
      enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK,
      {enterprise_management::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY},
      /*is_machine_level=*/false);

  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/false));
  // Unrelated extension versions are not blocked.
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallPolicyServiceTest,
                       CanInstallExtensionBlockedInBothLevels) {
  browser()->profile()->GetPrefs()->SetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled, true);

  SetExtensionInstallPolicy(
      kExtensionId1, kExtensionVersion1,
      enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK,
      {enterprise_management::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY},
      /*is_machine_level=*/true);

  SetExtensionInstallPolicy(
      kExtensionId1, kExtensionVersion1,
      enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK,
      {enterprise_management::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY},
      /*is_machine_level=*/false);

  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/false));
  // Unrelated extension versions are not blocked.
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
}

IN_PROC_BROWSER_TEST_F(
    ExtensionInstallPolicyServiceTest,
    CanInstallExtensionBlockedByUserLevelPolicyAllowedByMachineLevelPolicy) {
  browser()->profile()->GetPrefs()->SetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled, true);

  SetExtensionInstallPolicy(
      kExtensionId1, kExtensionVersion1,
      enterprise_management::ExtensionInstallPolicy::ACTION_ALLOW, {},
      /*is_machine_level=*/true);
  SetExtensionInstallPolicy(
      kExtensionId1, kExtensionVersion1,
      enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK,
      {enterprise_management::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY},
      /*is_machine_level=*/false);

  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/false));
  // Unrelated extension versions are not blocked.
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
}

IN_PROC_BROWSER_TEST_F(
    ExtensionInstallPolicyServiceTest,
    CanInstallExtensionAllowedByUserLevelPolicyBlockedByMachineLevelPolicy) {
  browser()->profile()->GetPrefs()->SetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled, true);

  SetExtensionInstallPolicy(
      kExtensionId1, kExtensionVersion1,
      enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK,
      {enterprise_management::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY},
      /*is_machine_level=*/true);
  SetExtensionInstallPolicy(
      kExtensionId1, kExtensionVersion1,
      enterprise_management::ExtensionInstallPolicy::ACTION_ALLOW, {},
      /*is_machine_level=*/false);

  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/false));
  // Unrelated extension versions are not blocked.
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallPolicyServiceTest,
                       CanInstallExtensionAllowedByMachineLevelPolicy) {
  browser()->profile()->GetPrefs()->SetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled, true);
  SetExtensionInstallPolicy(
      kExtensionId1, kExtensionVersion1,
      enterprise_management::ExtensionInstallPolicy::ACTION_ALLOW, {},
      /*is_machine_level=*/false);

  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/true));
  // Unrelated extension versions are not blocked.
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallPolicyServiceTest,
                       CanInstallExtensionAllowedByUserLevelPolicy) {
  browser()->profile()->GetPrefs()->SetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled, true);
  SetExtensionInstallPolicy(
      kExtensionId1, kExtensionVersion1,
      enterprise_management::ExtensionInstallPolicy::ACTION_ALLOW, {},
      /*is_machine_level=*/true);

  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/true));
  // Unrelated extension versions are not blocked.
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/true));
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId2,
                                                   kExtensionVersion2,
                                                   /*expected_result=*/true));
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallPolicyServiceTest,
                       CanInstallExtensionBlockedByExtensionSettings) {
  // Force-install `kExtensionId1`.
  std::string webstore_update_url =
      extension_urls::GetWebstoreUpdateUrl().spec();
  base::ListValue force_list;
  force_list.Append(base::StrCat({kExtensionId1, ";", webstore_update_url}));
  PolicyMap policies;
  policies.Set(key::kExtensionSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(force_list)), nullptr);
  UpdateProviderPolicy(policies);

  auto* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(extension_management);
  // CanInstallExtension() returns true even though the extension is blocked by
  // the ExtensionSettings policy. "true" here means "EIPS will not block it",
  // but other things still can (in this case,
  // StandardManagementPolicyProvider).
  ASSERT_NO_FATAL_FAILURE(CheckCanInstallExtension(kExtensionId1,
                                                   kExtensionVersion1,
                                                   /*expected_result=*/true));
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallPolicyServiceTest,
                       CanInstallExtensionServerUnreachable) {
  browser()->profile()->GetPrefs()->SetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled, true);
  SetExtensionInstallPolicy(
      kExtensionId1, kExtensionVersion1,
      enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK,
      {enterprise_management::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY},
      /*is_machine_level=*/true);
  test_server_.reset();
  ASSERT_NO_FATAL_FAILURE(CheckUserMayInstall(kExtensionId1, kExtensionVersion1,
                                              /*is_from_webstore=*/true,
                                              /*expected_result=*/true));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace policy
