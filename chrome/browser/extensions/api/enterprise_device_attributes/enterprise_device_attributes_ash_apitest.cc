// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_tags.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/extensions/api/force_installed_affiliated_extension_apitest.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "url/gurl.h"

namespace {

constexpr char kDeviceId[] = "device_id";
constexpr char kSerialNumber[] = "serial_number";
constexpr char kAssetId[] = "asset_id";
constexpr char kAnnotatedLocation[] = "annotated_location";
constexpr char kHostname[] = "hostname";

constexpr char kTestExtensionID[] = "nbiliclbejdndfpchgkbmfoppjplbdok";
constexpr char kExtensionPath[] =
    "extensions/api_test/enterprise_device_attributes/";
constexpr char kExtensionPemPath[] =
    "extensions/api_test/enterprise_device_attributes.pem";

base::Value::Dict BuildCustomArg(
    const std::string& expected_directory_device_id,
    const std::string& expected_serial_number,
    const std::string& expected_asset_id,
    const std::string& expected_annotated_location,
    const std::string& expected_hostname) {
  base::Value::Dict custom_arg;
  custom_arg.Set("expectedDirectoryDeviceId", expected_directory_device_id);
  custom_arg.Set("expectedSerialNumber", expected_serial_number);
  custom_arg.Set("expectedAssetId", expected_asset_id);
  custom_arg.Set("expectedAnnotatedLocation", expected_annotated_location);
  custom_arg.Set("expectedHostname", expected_hostname);
  return custom_arg;
}

}  // namespace

namespace extensions {

class EnterpriseDeviceAttributesTest
    : public ForceInstalledAffiliatedExtensionApiTest,
      public ::testing::WithParamInterface<bool> {
 public:
  EnterpriseDeviceAttributesTest()
      : ForceInstalledAffiliatedExtensionApiTest(GetParam()) {
    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  kSerialNumber);
  }

 protected:
  // ForceInstalledAffiliatedExtensionApiTest
  void SetUpInProcessBrowserTestFixture() override {
    ForceInstalledAffiliatedExtensionApiTest::
        SetUpInProcessBrowserTestFixture();

    // Init the device policy.
    policy::DevicePolicyBuilder* device_policy = test_helper_.device_policy();
    device_policy->SetDefaultSigningKey();
    device_policy->policy_data().set_directory_api_id(kDeviceId);
    device_policy->policy_data().set_annotated_asset_id(kAssetId);
    device_policy->policy_data().set_annotated_location(kAnnotatedLocation);
    enterprise_management::NetworkHostnameProto* proto =
        device_policy->payload().mutable_network_hostname();
    proto->set_device_hostname_template(kHostname);
    device_policy->Build();

    ash::FakeSessionManagerClient::Get()->set_device_policy(
        device_policy->GetBlob());
    ash::FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
  }

 private:
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

IN_PROC_BROWSER_TEST_P(EnterpriseDeviceAttributesTest, PRE_Success) {
  policy::AffiliationTestHelper::PreLoginUser(affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_P(EnterpriseDeviceAttributesTest, Success) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-be4e8241-2469-4b3f-969e-026494fb4ced");

  const bool is_affiliated = GetParam();
  EXPECT_EQ(is_affiliated, user_manager::UserManager::Get()
                               ->FindUser(affiliation_mixin_.account_id())
                               ->IsAffiliated());

  const Extension* extension =
      ForceInstallExtension(kExtensionPath, kExtensionPemPath);
  const GURL test_url = extension->GetResourceURL("basic.html");

  // Device attributes are available only for affiliated user.
  std::string expected_directory_device_id = is_affiliated ? kDeviceId : "";
  std::string expected_serial_number = is_affiliated ? kSerialNumber : "";
  std::string expected_asset_id = is_affiliated ? kAssetId : "";
  std::string expected_annotated_location =
      is_affiliated ? kAnnotatedLocation : "";
  std::string expected_hostname = is_affiliated ? kHostname : "";
  TestExtension(CreateBrowser(profile()), test_url,
                BuildCustomArg(expected_directory_device_id,
                               expected_serial_number, expected_asset_id,
                               expected_annotated_location, expected_hostname));
}

// Both cases of affiliated and non-affiliated users are tested.
INSTANTIATE_TEST_SUITE_P(AffiliationCheck,
                         EnterpriseDeviceAttributesTest,
                         ::testing::Bool());

// Ensure that extensions that are not pre-installed by policy throw an install
// warning if they request the enterprise.deviceAttributes permission in the
// manifest and that such extensions don't see the
// chrome.enterprise.deviceAttributes namespace.
IN_PROC_BROWSER_TEST_F(
    ExtensionApiTest,
    EnterpriseDeviceAttributesIsRestrictedToPolicyExtension) {
  ASSERT_TRUE(RunExtensionTest("enterprise_device_attributes",
                               {.extension_url = "api_not_available.html"},
                               {.ignore_manifest_warnings = true}));

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile())
          ->enabled_extensions()
          .GetByID(kTestExtensionID);
  ASSERT_FALSE(extension->install_warnings().empty());
  EXPECT_EQ(
      "'enterprise.deviceAttributes' is not allowed for specified install "
      "location.",
      extension->install_warnings()[0].message);
}

}  //  namespace extensions
