// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/policy_test_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/result_catcher.h"

namespace {

constexpr char kDeviceId[] = "device_id";
constexpr char kSerialNumber[] = "serial_number";
constexpr char kAssetId[] = "asset_id";
constexpr char kAnnotatedLocation[] = "annotated_location";
constexpr char kUpdateManifestPath[] =
    "/extensions/api_test/enterprise_device_attributes/update_manifest.xml";

constexpr char kAffiliatedUserEmail[] = "user@example.com";
constexpr char kAffiliatedUserGaiaId[] = "1029384756";
constexpr char kAffiliationID[] = "some-affiliation-id";
constexpr char kAnotherAffiliationID[] = "another-affiliation-id";

// The managed_storage extension has a key defined in its manifest, so that
// its extension ID is well-known and the policy system can push policies for
// the extension.
constexpr char kTestExtensionID[] = "nbiliclbejdndfpchgkbmfoppjplbdok";

struct Params {
  explicit Params(bool affiliated) : affiliated(affiliated) {}
  bool affiliated;
};

// Must be a valid test name (no spaces etc.). Makes the test show up as e.g.
// AffiliationCheck/U.A.B.T.Affiliated/NotAffiliated_NotActiveDirectory
std::string PrintParam(testing::TestParamInfo<Params> param_info) {
  return base::StringPrintf("%sAffiliated",
                            param_info.param.affiliated ? "" : "Not");
}

base::Value BuildCustomArg(const std::string& expected_directory_device_id,
                           const std::string& expected_serial_number,
                           const std::string& expected_asset_id,
                           const std::string& expected_annotated_location) {
  base::Value custom_arg(base::Value::Type::DICTIONARY);
  custom_arg.SetKey("expectedDirectoryDeviceId",
                    base::Value(expected_directory_device_id));
  custom_arg.SetKey("expectedSerialNumber",
                    base::Value(expected_serial_number));
  custom_arg.SetKey("expectedAssetId", base::Value(expected_asset_id));
  custom_arg.SetKey("expectedAnnotatedLocation",
                    base::Value(expected_annotated_location));
  return custom_arg;
}

}  // namespace

namespace extensions {

class EnterpriseDeviceAttributesTest
    : public ExtensionApiTest,
      public ::testing::WithParamInterface<Params> {
 public:
  EnterpriseDeviceAttributesTest() {
    fake_statistics_provider_.SetMachineStatistic(
        chromeos::system::kSerialNumberKeyForTest, kSerialNumber);
    set_exit_when_last_browser_closes(false);
    set_chromeos_user_ = false;
  }

 protected:
  // ExtensionApiTest
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

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
    if (GetParam().affiliated) {
      user_affiliation_ids.insert(kAffiliationID);
    } else {
      user_affiliation_ids.insert(kAnotherAffiliationID);
    }
    policy::UserPolicyBuilder user_policy;
    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetUserAffiliationIDs(
        &user_policy, affiliated_account_id_, user_affiliation_ids));

    test_helper_.InstallOwnerKey();
    // Init the device policy.
    policy::DevicePolicyBuilder* device_policy = test_helper_.device_policy();
    device_policy->SetDefaultSigningKey();
    device_policy->policy_data().set_directory_api_id(kDeviceId);
    device_policy->policy_data().set_annotated_asset_id(kAssetId);
    device_policy->policy_data().set_annotated_location(kAnnotatedLocation);
    device_policy->Build();

    chromeos::FakeSessionManagerClient::Get()->set_device_policy(
        device_policy->GetBlob());
    chromeos::FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);

    // Init the user policy provider.
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    // Set retry delay to prevent timeouts.
    policy::DeviceManagementService::SetRetryDelayForTesting(0);
  }

  void SetUpOnMainThread() override {
    const base::ListValue* users =
        g_browser_process->local_state()->GetList("LoggedInUsers");
    if (!users->empty())
      policy::AffiliationTestHelper::LoginUser(affiliated_account_id_);

    ExtensionApiTest::SetUpOnMainThread();
  }

  // Load |page_url| in |browser| and wait for PASSED or FAILED notification.
  // The functionality of this function is reduced functionality of
  // RunExtensionSubtest(), but we don't use it here because it requires
  // function InProcessBrowserTest::browser() to return non-NULL pointer.
  // Unfortunately it returns the value which is set in constructor and can't be
  // modified. Because on login flow there is no browser, the function
  // InProcessBrowserTest::browser() always returns NULL. Besides this we need
  // only very little functionality from RunExtensionSubtest(). Thus so that
  // don't make RunExtensionSubtest() to complex we just introduce a new
  // function.
  bool TestExtension(Browser* browser,
                     const std::string& page_url,
                     const base::Value& custom_arg_value) {
    DCHECK(!page_url.empty()) << "page_url cannot be empty";

    std::string custom_arg;
    base::JSONWriter::Write(custom_arg_value, &custom_arg);
    SetCustomArg(custom_arg);

    extensions::ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser, GURL(page_url));

    if (!catcher.GetNextResult()) {
      message_ = catcher.message();
      return false;
    }
    return true;
  }

  const AccountId affiliated_account_id_ =
      AccountId::FromUserEmailGaiaId(kAffiliatedUserEmail,
                                     kAffiliatedUserGaiaId);

  policy::MockConfigurationPolicyProvider policy_provider_;

 private:
  chromeos::ScopedStubInstallAttributes test_install_attributes_{
      chromeos::StubInstallAttributes::CreateCloudManaged("fake-domain",
                                                          "fake-id")};
  policy::DevicePolicyCrosTestHelper test_helper_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

IN_PROC_BROWSER_TEST_P(EnterpriseDeviceAttributesTest, PRE_Success) {
  policy::AffiliationTestHelper::PreLoginUser(affiliated_account_id_);
}

IN_PROC_BROWSER_TEST_P(EnterpriseDeviceAttributesTest, Success) {
  policy_test_utils::SetUpEmbeddedTestServer(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  policy_test_utils::SetExtensionInstallForcelistPolicy(
      kTestExtensionID, embedded_test_server()->GetURL(kUpdateManifestPath),
      profile(), &policy_provider_);

  EXPECT_EQ(GetParam().affiliated, user_manager::UserManager::Get()
                                       ->FindUser(affiliated_account_id_)
                                       ->IsAffiliated());

  // Device attributes are available only for affiliated user.
  std::string expected_directory_device_id =
      GetParam().affiliated ? kDeviceId : "";
  std::string expected_serial_number =
      GetParam().affiliated ? kSerialNumber : "";
  std::string expected_asset_id = GetParam().affiliated ? kAssetId : "";
  std::string expected_annotated_location =
      GetParam().affiliated ? kAnnotatedLocation : "";

  // Pass the expected value (device_id) to test.
  ASSERT_TRUE(TestExtension(
      CreateBrowser(profile()),
      base::StringPrintf("chrome-extension://%s/basic.html", kTestExtensionID),
      BuildCustomArg(expected_directory_device_id, expected_serial_number,
                     expected_asset_id, expected_annotated_location)))
      << message_;
}

// Ensure that extensions that are not pre-installed by policy throw an install
// warning if they request the enterprise.deviceAttributes permission in the
// manifest and that such extensions don't see the
// chrome.enterprise.deviceAttributes namespace.
IN_PROC_BROWSER_TEST_F(
    ExtensionApiTest,
    EnterpriseDeviceAttributesIsRestrictedToPolicyExtension) {
  ASSERT_TRUE(RunExtensionSubtest("enterprise_device_attributes",
                                  "api_not_available.html",
                                  kFlagIgnoreManifestWarnings));

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("enterprise_device_attributes");
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  const extensions::Extension* extension =
      GetExtensionByPath(registry->enabled_extensions(), extension_path);
  ASSERT_FALSE(extension->install_warnings().empty());
  EXPECT_EQ(
      "'enterprise.deviceAttributes' is not allowed for specified install "
      "location.",
      extension->install_warnings()[0].message);
}

// Both cases of affiliated and non-affiliated on the device user are tested.
INSTANTIATE_TEST_SUITE_P(AffiliationCheck,
                         EnterpriseDeviceAttributesTest,
                         ::testing::Values(Params(true /* affiliated */),
                                           Params(false /* affiliated */)),
                         PrintParam);
}  //  namespace extensions
