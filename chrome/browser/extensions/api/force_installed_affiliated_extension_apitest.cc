// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/force_installed_affiliated_extension_apitest.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/result_catcher.h"

namespace {

base::FilePath GetTestDataDir() {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA);
}

}  // namespace

namespace extensions {

ForceInstalledAffiliatedExtensionApiTest::
    ForceInstalledAffiliatedExtensionApiTest(bool is_affiliated)
    : test_install_attributes_(
          ash::StubInstallAttributes::CreateCloudManaged("fake-domain",
                                                         "fake-id")) {
  set_exit_when_last_browser_closes(false);
  set_chromeos_user_ = false;
  affiliation_mixin_.set_affiliated(is_affiliated);
  cryptohome_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
  cryptohome_mixin_.ApplyAuthConfig(
      affiliation_mixin_.account_id(),
      ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));
}

ForceInstalledAffiliatedExtensionApiTest::
    ~ForceInstalledAffiliatedExtensionApiTest() = default;

void ForceInstalledAffiliatedExtensionApiTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
      command_line);
  MixinBasedExtensionApiTest::SetUpCommandLine(command_line);
}

void ForceInstalledAffiliatedExtensionApiTest::
    SetUpInProcessBrowserTestFixture() {
  // Initialize clients here so they are available during setup. They will be
  // shutdown in ChromeBrowserMain.
  ash::SessionManagerClient::InitializeFakeInMemory();

  // Init the user policy provider.
  policy_provider_.SetDefaultReturns(
      /*is_initialization_complete_return=*/true,
      /*is_first_policy_load_complete_return=*/true);
  policy_provider_.SetAutoRefresh();
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
      &policy_provider_);

  // Set retry delay to prevent timeouts.
  policy::DeviceManagementService::SetRetryDelayForTesting(0);

  MixinBasedExtensionApiTest::SetUpInProcessBrowserTestFixture();
}

void ForceInstalledAffiliatedExtensionApiTest::SetUpOnMainThread() {
  // Log in user that was created with
  // policy::AffiliationTestHelper::PreLoginUser() in the PRE_ test.
  const base::Value::List& users =
      g_browser_process->local_state()->GetList("LoggedInUsers");
  if (!users.empty()) {
    policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  }

  force_install_mixin_.InitWithMockPolicyProvider(profile(), &policy_provider_);

  MixinBasedExtensionApiTest::SetUpOnMainThread();
}

const extensions::Extension*
ForceInstalledAffiliatedExtensionApiTest::ForceInstallExtension(
    const std::string& extension_path,
    const std::string& pem_path) {
  extensions::ExtensionId extension_id;
  EXPECT_TRUE(force_install_mixin_.ForceInstallFromSourceDir(
      GetTestDataDir().AppendASCII(extension_path),
      GetTestDataDir().AppendASCII(pem_path),
      ExtensionForceInstallMixin::WaitMode::kLoad, &extension_id));
  return force_install_mixin_.GetInstalledExtension(extension_id);
}

void ForceInstalledAffiliatedExtensionApiTest::TestExtension(
    Browser* browser,
    const GURL& page_url,
    const base::Value::Dict& custom_arg_value) {
  DCHECK(page_url.is_valid()) << "page_url must be valid";

  std::string custom_arg;
  base::JSONWriter::Write(custom_arg_value, &custom_arg);
  SetCustomArg(custom_arg);

  extensions::ResultCatcher catcher;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, GURL(page_url)));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  //  namespace extensions
