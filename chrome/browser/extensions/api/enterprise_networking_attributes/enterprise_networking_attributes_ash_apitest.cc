// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/extensions/api/force_installed_affiliated_extension_apitest.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "content/public/test/browser_test.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "url/gurl.h"

namespace {

const char kErrorUserNotAffiliated[] =
    "Network attributes can only be read by an affiliated user.";
const char kErrorNetworkNotConnected[] =
    "Device is not connected to a network.";

constexpr char kTestExtensionID[] = "pkhdjpcjgonhlomdjmnddhbfpgkdhgle";
constexpr char kExtensionPath[] =
    "extensions/api_test/enterprise_networking_attributes/";
constexpr char kExtensionPemPath[] =
    "extensions/api_test/enterprise_networking_attributes.pem";

constexpr char kMacAddress[] = "0123456789AB";
constexpr char kFormattedMacAddress[] = "01:23:45:67:89:AB";
constexpr char kIpv4Address[] = "192.168.0.42";
constexpr char kIpv6Address[] = "fe80::1262:d0ff:fef5:e8a9";

constexpr char kWifiDevicePath[] = "/device/stub_wifi";
constexpr char kWifiServicePath[] = "/service/stub_wifi";
constexpr char kWifiIPConfigV4Path[] = "/ipconfig/stub_wifi-ipv4";
constexpr char kWifiIPConfigV6Path[] = "/ipconfig/stub_wifi-ipv6";

base::Value::Dict BuildCustomArgForSuccess(
    const std::string& expected_mac_address,
    const std::string& expected_ipv4_address,
    const std::string& expected_ipv6_address) {
  base::Value::Dict network_details;
  network_details.Set("macAddress", expected_mac_address);
  network_details.Set("ipv4", expected_ipv4_address);
  network_details.Set("ipv6", expected_ipv6_address);

  base::Value::Dict custom_arg;
  custom_arg.Set("testName", "success");
  custom_arg.Set("expectedResult", std::move(network_details));
  return custom_arg;
}

base::Value::Dict BuildCustomArgForFailure(
    const std::string& expected_error_message) {
  base::Value::Dict custom_arg;
  custom_arg.Set("testName", "failure");
  custom_arg.Set("expectedErrorMessage", expected_error_message);
  return custom_arg;
}

}  // namespace

namespace extensions {

class EnterpriseNetworkingAttributesTest
    : public ForceInstalledAffiliatedExtensionApiTest,
      public ::testing::WithParamInterface<bool> {
 public:
  EnterpriseNetworkingAttributesTest()
      : ForceInstalledAffiliatedExtensionApiTest(GetParam()) {}

  void SetupDisconnectedNetwork() {
    ash::ShillDeviceClient::TestInterface* shill_device_client =
        ash::ShillDeviceClient::Get()->GetTestInterface();
    ash::ShillIPConfigClient::TestInterface* shill_ipconfig_client =
        ash::ShillIPConfigClient::Get()->GetTestInterface();
    ash::ShillServiceClient::TestInterface* shill_service_client =
        ash::ShillServiceClient::Get()->GetTestInterface();
    ash::ShillProfileClient::TestInterface* shill_profile_client =
        ash::ShillProfileClient::Get()->GetTestInterface();

    shill_service_client->ClearServices();
    shill_device_client->ClearDevices();

    shill_device_client->AddDevice(kWifiDevicePath, shill::kTypeWifi,
                                   "stub_wifi_device");
    shill_device_client->SetDeviceProperty(
        kWifiDevicePath, shill::kAddressProperty, base::Value(kMacAddress),
        /* notify_changed= */ false);

    base::Value::Dict ipconfig_v4_dictionary;
    ipconfig_v4_dictionary.Set(shill::kAddressProperty, kIpv4Address);
    ipconfig_v4_dictionary.Set(shill::kMethodProperty, shill::kTypeIPv4);
    shill_ipconfig_client->AddIPConfig(kWifiIPConfigV4Path,
                                       std::move(ipconfig_v4_dictionary));

    base::Value::Dict ipconfig_v6_dictionary;
    ipconfig_v6_dictionary.Set(shill::kAddressProperty, kIpv6Address);
    ipconfig_v6_dictionary.Set(shill::kMethodProperty, shill::kTypeIPv6);
    shill_ipconfig_client->AddIPConfig(kWifiIPConfigV6Path,
                                       std::move(ipconfig_v6_dictionary));

    base::Value::List ip_configs;
    ip_configs.Append(kWifiIPConfigV4Path);
    ip_configs.Append(kWifiIPConfigV6Path);
    shill_device_client->SetDeviceProperty(kWifiDevicePath,
                                           shill::kIPConfigsProperty,
                                           base::Value(std::move(ip_configs)),
                                           /*notify_changed=*/false);

    shill_service_client->AddService(kWifiServicePath, "wifi_guid",
                                     "wifi_network_name", shill::kTypeWifi,
                                     shill::kStateIdle, /* visible= */ true);
    shill_service_client->SetServiceProperty(
        kWifiServicePath, shill::kConnectableProperty, base::Value(true));

    shill_profile_client->AddService(
        ash::ShillProfileClient::GetSharedProfilePath(), kWifiServicePath);

    base::RunLoop().RunUntilIdle();
  }

  void ConnectNetwork() {
    ash::ShillServiceClient::TestInterface* shill_service_client =
        ash::ShillServiceClient::Get()->GetTestInterface();
    shill_service_client->SetServiceProperty(kWifiServicePath,
                                             shill::kStateProperty,
                                             base::Value(shill::kStateOnline));
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(EnterpriseNetworkingAttributesTest,
                       PRE_GetNetworkDetails) {
  policy::AffiliationTestHelper::PreLoginUser(affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_P(EnterpriseNetworkingAttributesTest, GetNetworkDetails) {
  const bool is_affiliated = GetParam();
  EXPECT_EQ(is_affiliated, user_manager::UserManager::Get()
                               ->FindUser(affiliation_mixin_.account_id())
                               ->IsAffiliated());

  const Extension* extension =
      ForceInstallExtension(kExtensionPath, kExtensionPemPath);
  SetupDisconnectedNetwork();
  const GURL test_url = extension->GetResourceURL("test.html");

  // Run test without connected network.
  base::Value::Dict custom_arg_disconnected =
      is_affiliated ? BuildCustomArgForFailure(kErrorNetworkNotConnected)
                    : BuildCustomArgForFailure(kErrorUserNotAffiliated);
  TestExtension(CreateBrowser(profile()), test_url,
                std::move(custom_arg_disconnected));

  // Run test with connected network.
  ConnectNetwork();
  base::Value::Dict custom_arg_connected =
      is_affiliated ? BuildCustomArgForSuccess(kFormattedMacAddress,
                                               kIpv4Address, kIpv6Address)
                    : BuildCustomArgForFailure(kErrorUserNotAffiliated);
  TestExtension(CreateBrowser(profile()), test_url,
                std::move(custom_arg_connected));
}

// Both cases of affiliated and non-affiliated users are tested.
INSTANTIATE_TEST_SUITE_P(AffiliationCheck,
                         EnterpriseNetworkingAttributesTest,
                         ::testing::Bool());

// Ensure that extensions that are not pre-installed by policy throw an install
// warning if they request the enterprise.networkingAttributes permission in the
// manifest and that such extensions don't see the
// chrome.enterprise.networkingAttributes namespace.
IN_PROC_BROWSER_TEST_F(
    ExtensionApiTest,
    EnterpriseNetworkingAttributesIsRestrictedToPolicyExtension) {
  ASSERT_TRUE(RunExtensionTest("enterprise_networking_attributes",
                               {.extension_url = "api_not_available.html"},
                               {.ignore_manifest_warnings = true}));

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("enterprise_networking_attributes");
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile())
          ->enabled_extensions()
          .GetByID(kTestExtensionID);
  ASSERT_FALSE(extension->install_warnings().empty());
  EXPECT_EQ(
      "'enterprise.networkingAttributes' is not allowed for specified install "
      "location.",
      extension->install_warnings()[0].message);
}

}  //  namespace extensions
