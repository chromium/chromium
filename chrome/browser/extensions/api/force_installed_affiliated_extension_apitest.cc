// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/force_installed_affiliated_extension_apitest.h"

#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/result_catcher.h"

namespace {

// If running with |is_affiliated|==true, the test will use the same
// |kAffiliationID| as user and device affiliation ID, which makes the user
// affiliated (affiliation IDs overlap).
// If running with |is_affiliated|==false, the test will use |kAffiliationID| as
// device and |kAnotherAffiliationID| as user affiliation ID, which makes the
// user non-affiliated (affiliation IDs don't overlap).
constexpr char kAffiliationID[] = "some-affiliation-id";
constexpr char kAnotherAffiliationID[] = "another-affiliation-id";

constexpr char kAffiliatedUserEmail[] = "user@example.com";
constexpr char kAffiliatedUserGaiaId[] = "1029384756";

base::FilePath GetTestDataDir() {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA);
}

}  // namespace

namespace extensions {

ForceInstalledAffiliatedExtensionApiTest::
    ForceInstalledAffiliatedExtensionApiTest(bool is_affiliated)
    : is_affiliated_(is_affiliated),
      affiliated_account_id_(
          AccountId::FromUserEmailGaiaId(kAffiliatedUserEmail,
                                         kAffiliatedUserGaiaId)),
      test_install_attributes_(
          chromeos::StubInstallAttributes::CreateCloudManaged("fake-domain",
                                                              "fake-id")) {
  set_exit_when_last_browser_closes(false);
  set_chromeos_user_ = false;
}

ForceInstalledAffiliatedExtensionApiTest::
    ~ForceInstalledAffiliatedExtensionApiTest() = default;

void ForceInstalledAffiliatedExtensionApiTest::SetUp() {
  mixin_host_.SetUp();
  ExtensionApiTest::SetUp();
}

void ForceInstalledAffiliatedExtensionApiTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  mixin_host_.SetUpCommandLine(command_line);
  policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
      command_line);
  ExtensionApiTest::SetUpCommandLine(command_line);
}

void ForceInstalledAffiliatedExtensionApiTest::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  mixin_host_.SetUpDefaultCommandLine(command_line);
  ExtensionApiTest::SetUpDefaultCommandLine(command_line);
}

bool ForceInstalledAffiliatedExtensionApiTest::SetUpUserDataDirectory() {
  return mixin_host_.SetUpUserDataDirectory() &&
         ExtensionApiTest::SetUpUserDataDirectory();
}

void ForceInstalledAffiliatedExtensionApiTest::
    SetUpInProcessBrowserTestFixture() {
  mixin_host_.SetUpInProcessBrowserTestFixture();

  // Initialize clients here so they are available during setup. They will be
  // shutdown in ChromeBrowserMain.
  chromeos::SessionManagerClient::InitializeFakeInMemory();
  policy::AffiliationTestHelper affiliation_helper =
      policy::AffiliationTestHelper::CreateForCloud(
          chromeos::FakeSessionManagerClient::Get());

  std::set<std::string> device_affiliation_ids;
  device_affiliation_ids.insert(kAffiliationID);
  ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetDeviceAffiliationIDs(
      &test_helper_, device_affiliation_ids));
  test_helper_.InstallOwnerKey();

  std::set<std::string> user_affiliation_ids;
  if (is_affiliated_) {
    user_affiliation_ids.insert(kAffiliationID);
  } else {
    user_affiliation_ids.insert(kAnotherAffiliationID);
  }
  policy::UserPolicyBuilder user_policy;
  ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetUserAffiliationIDs(
      &user_policy, affiliated_account_id_, user_affiliation_ids));
  test_helper_.InstallOwnerKey();

  // Init the user policy provider.
  EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
      .WillRepeatedly(testing::Return(true));
  policy_provider_.SetAutoRefresh();
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
      &policy_provider_);

  // Set retry delay to prevent timeouts.
  policy::DeviceManagementService::SetRetryDelayForTesting(0);

  ExtensionApiTest::SetUpInProcessBrowserTestFixture();
}

void ForceInstalledAffiliatedExtensionApiTest::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  mixin_host_.CreatedBrowserMainParts(browser_main_parts);
  ExtensionApiTest::CreatedBrowserMainParts(browser_main_parts);
}

void ForceInstalledAffiliatedExtensionApiTest::SetUpOnMainThread() {
  mixin_host_.SetUpOnMainThread();
  // Log in user that was created with
  // policy::AffiliationTestHelper::PreLoginUser() in the PRE_ test.
  const base::ListValue* users =
      g_browser_process->local_state()->GetList("LoggedInUsers");
  if (!users->empty()) {
    policy::AffiliationTestHelper::LoginUser(affiliated_account_id_);
  }

  force_install_mixin_.InitWithMockPolicyProvider(profile(), &policy_provider_);
  ExtensionApiTest::SetUpOnMainThread();
}

void ForceInstalledAffiliatedExtensionApiTest::TearDownOnMainThread() {
  mixin_host_.TearDownOnMainThread();
  ExtensionApiTest::TearDownOnMainThread();
}

void ForceInstalledAffiliatedExtensionApiTest::
    TearDownInProcessBrowserTestFixture() {
  mixin_host_.TearDownInProcessBrowserTestFixture();
  ExtensionApiTest::TearDownInProcessBrowserTestFixture();
}

void ForceInstalledAffiliatedExtensionApiTest::TearDown() {
  mixin_host_.TearDown();
  ExtensionApiTest::TearDown();
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
    const base::Value& custom_arg_value) {
  DCHECK(page_url.is_valid()) << "page_url must be valid";

  std::string custom_arg;
  base::JSONWriter::Write(custom_arg_value, &custom_arg);
  SetCustomArg(custom_arg);

  extensions::ResultCatcher catcher;
  ui_test_utils::NavigateToURL(browser, GURL(page_url));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  //  namespace extensions
