// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/extension_install_policy_service.h"

#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_delegate_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_client_types.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {
constexpr char kExtensionId1[] = "extension1";
constexpr char kExtensionVersion1[] = "1.0.0.0";
constexpr char kExtensionId2[] = "extension2";
constexpr char kExtensionVersion2[] = "2.0.0.0";
constexpr char kEnrollmentToken[] = "enrollment_token";
constexpr char kClientID[] = "fake_client_id";
constexpr char kDMToken[] = "fake_dm_token";

ClientStorage::ClientInfo CreateTestClientInfo() {
  ClientStorage::ClientInfo client_info;
  client_info.device_id = kClientID;
  client_info.device_token = kDMToken;
  client_info.allowed_policy_types.insert(
      {dm_protocol::kChromeMachineLevelUserCloudPolicyType,
       dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
       dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType});
  return client_info;
}
}  // namespace

class ExtensionInstallPolicyServiceTest : public PlatformBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(policy_server_->Start());
  }

  std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_server_ =
      std::make_unique<policy::EmbeddedPolicyTestServer>();
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kEnableExtensionInstallPolicyFetching};
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallPolicyServiceTest,
                       CanInstallExtensionAllowedByDefault) {
  ExtensionInstallPolicyService service(browser()->profile());
  base::test::TestFuture<bool> future;
  service.CanInstallExtension(
      ExtensionIdAndVersion(kExtensionId1, kExtensionVersion1),
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
}

#if !BUILDFLAG(IS_CHROMEOS)
class MachineLevelExtensionInstallPolicyFetchTest : public PlatformBrowserTest {
 public:
  MachineLevelExtensionInstallPolicyFetchTest() {
    BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.SetEnrollmentToken(kEnrollmentToken);
    storage_.SetClientId(kClientID);
    storage_.EnableStorage(true);
    storage_.SetDMToken(kDMToken);
  }
  MachineLevelExtensionInstallPolicyFetchTest(
      const MachineLevelExtensionInstallPolicyFetchTest&) = delete;
  MachineLevelExtensionInstallPolicyFetchTest& operator=(
      const MachineLevelExtensionInstallPolicyFetchTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    SetUpTestServer();
    ASSERT_TRUE(test_server_->Start());

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl,
                                    test_server_->GetServiceURL().spec());
    ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    PlatformBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableChromeBrowserCloudManagement);
  }
#endif

  void SetUpTestServer() {
    test_server_ = std::make_unique<EmbeddedPolicyTestServer>();
    test_server_->client_storage()->RegisterClient(CreateTestClientInfo());
  }

  DMToken retrieve_dm_token() { return storage_.RetrieveDMToken(); }

 protected:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<EmbeddedPolicyTestServer> test_server_;
  FakeBrowserDMTokenStorage storage_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kEnableExtensionInstallPolicyFetching};
};

IN_PROC_BROWSER_TEST_F(MachineLevelExtensionInstallPolicyFetchTest,
                       CanInstallExtensionBlockedByMachineLevelPolicy) {
  MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  ASSERT_TRUE(manager);
  browser()->profile()->GetPrefs()->SetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled, true);

  enterprise_management::ExtensionInstallPolicies extension_install_policies;
  {
    enterprise_management::ExtensionInstallPolicy* policy =
        extension_install_policies.add_policies();
    policy->set_extension_id(kExtensionId1);
    policy->set_extension_version(kExtensionVersion1);
    policy->set_action(
        enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK);
    policy->add_reasons(
        enterprise_management::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY);
  }

  test_server_->policy_storage()->SetPolicyPayload(
      dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType,
      base::StrCat({kExtensionId1, "@", kExtensionVersion1}),
      extension_install_policies.SerializeAsString());
  test_server_->policy_storage()->set_robot_api_auth_code("fake_auth_code");
  test_server_->policy_storage()->set_service_account_identity("foo@bar.com");

  ExtensionInstallPolicyService service(browser()->profile());
  // Blocked extension and version should not be allowed to install.
  {
    base::test::TestFuture<bool> future;
    service.CanInstallExtension(
        ExtensionIdAndVersion(kExtensionId1, kExtensionVersion1),
        future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_FALSE(future.Get());
  }
  // Allowed extension should be allowed to install.
  {
    base::test::TestFuture<bool> future;
    service.CanInstallExtension(
        ExtensionIdAndVersion(kExtensionId2, kExtensionVersion1),
        future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_TRUE(future.Get());
  }
  // Allowed extension and version should be allowed to install.
  {
    base::test::TestFuture<bool> future;
    service.CanInstallExtension(
        ExtensionIdAndVersion(kExtensionId1, kExtensionVersion2),
        future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_TRUE(future.Get());
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace policy
