// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/force_installed_affiliated_extension_apitest.h"

#include "base/json/json_writer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/extensions/policy_test_utils.h"
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

void ForceInstalledAffiliatedExtensionApiTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ExtensionApiTest::SetUpCommandLine(command_line);
  policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
      command_line);
}

void ForceInstalledAffiliatedExtensionApiTest::
    SetUpInProcessBrowserTestFixture() {
  ExtensionApiTest::SetUpInProcessBrowserTestFixture();

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
}

void ForceInstalledAffiliatedExtensionApiTest::SetUpOnMainThread() {
  // Log in user that was created with
  // policy::AffiliationTestHelper::PreLoginUser() in the PRE_ test.
  const base::ListValue* users =
      g_browser_process->local_state()->GetList("LoggedInUsers");
  if (!users->empty()) {
    policy::AffiliationTestHelper::LoginUser(affiliated_account_id_);
  }

  policy_test_utils::SetUpEmbeddedTestServer(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  ExtensionApiTest::SetUpOnMainThread();
}

const extensions::Extension*
ForceInstalledAffiliatedExtensionApiTest::ForceInstallExtension(
    const extensions::ExtensionId& extension_id,
    const std::string& update_manifest_path) {
  policy_test_utils::SetExtensionInstallForcelistPolicy(
      extension_id, embedded_test_server()->GetURL(update_manifest_path),
      profile(), &policy_provider_);
  const extensions::Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_id);
  DCHECK(extension);
  return extension;
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
