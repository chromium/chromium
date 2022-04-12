// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_ui_delegate_chromeos.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/network/cellular_metrics_logger.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_certificate_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/onc/network_onc_utils.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "dbus/object_path.h"
#include "extensions/browser/api/networking_private/networking_private_chromeos.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "extensions/common/switches.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

// This tests the Chrome OS implementation of the networkingPrivate API
// (NetworkingPrivateChromeOS). Note: The test expectations for chromeos, and
// win/mac (NetworkingPrivateServiceClient) are different to reflect the
// different implementations, but should be kept similar where possible.

using testing::Return;
using testing::_;

using chromeos::ShillDeviceClient;
using chromeos::ShillIPConfigClient;
using chromeos::ShillManagerClient;
using chromeos::ShillProfileClient;
using chromeos::ShillServiceClient;
using chromeos::UserDataAuthClient;

using extensions::NetworkingPrivateDelegate;
using extensions::NetworkingPrivateDelegateFactory;
using extensions::NetworkingPrivateChromeOS;

namespace {

const char kUser1ProfilePath[] = "/profile/user1/shill";
const char kEthernetDevicePath[] = "/device/stub_ethernet_device";
const char kWifiDevicePath[] = "/device/stub_wifi_device1";
const char kCellularDevicePath[] = "/device/stub_cellular_device1";
const char kIPConfigPath[] = "/ipconfig/ipconfig1";

const char kWifi1ServicePath[] = "stub_wifi1";
const char kWifi2ServicePath[] = "stub_wifi2";
const char kCellular1ServicePath[] = "stub_cellular1";

class UIDelegateStub : public NetworkingPrivateDelegate::UIDelegate {
 public:
  static int s_show_account_details_called_;

 private:
  // UIDelegate
  void ShowAccountDetails(const std::string& guid) const override {
    ++s_show_account_details_called_;
  }
};

// static
int UIDelegateStub::s_show_account_details_called_ = 0;

class NetworkingPrivateChromeOSApiTest : public extensions::ExtensionApiTest {
 public:
  NetworkingPrivateChromeOSApiTest() {}

  NetworkingPrivateChromeOSApiTest(const NetworkingPrivateChromeOSApiTest&) =
      delete;
  NetworkingPrivateChromeOSApiTest& operator=(
      const NetworkingPrivateChromeOSApiTest&) = delete;

  bool RunNetworkingSubtest(const std::string& test) {
    const std::string arg =
        base::StringPrintf("{\"test\": \"%s\"}", test.c_str());
    return RunExtensionTest(
        "networking_private/chromeos",
        {.custom_arg = arg.c_str(), .launch_as_platform_app = true});
  }

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    // Allowlist the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        "epcifkihnkjgphfkloaaleeakhpmgdmn");

    // TODO(pneubeck): Remove the following hack, once the NetworkingPrivateAPI
    // uses the ProfileHelper to obtain the userhash crbug/238623.
    cryptohome::AccountIdentifier login_user;
    login_user.set_account_id(user_manager::CanonicalizeUserID(
        command_line->GetSwitchValueNative(ash::switches::kLoginUser)));
    const std::string sanitized_user =
        UserDataAuthClient::GetStubSanitizedUsername(login_user);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    sanitized_user);
  }

  void InitializeSanitizedUsername() {
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    user_manager::User* user = user_manager->GetActiveUser();
    CHECK(user);
    std::string userhash;
    ::user_data_auth::GetSanitizedUsernameRequest request;
    request.set_username(
        cryptohome::CreateAccountIdentifierFromAccountId(user->GetAccountId())
            .account_id());
    chromeos::CryptohomeMiscClient::Get()->GetSanitizedUsername(
        request,
        base::BindOnce(
            [](std::string* out,
               absl::optional<::user_data_auth::GetSanitizedUsernameReply>
                   result) {
              CHECK(result.has_value());
              *out = result->sanitized_username();
            },
            &userhash_));
    content::RunAllPendingInMessageLoop();
    CHECK(!userhash_.empty());
  }

  void SetupCellular() {
    UIDelegateStub::s_show_account_details_called_ = 0;

    // Add a Cellular GSM Device.
    device_test()->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                             "stub_cellular_device1");
    base::DictionaryValue home_provider;
    home_provider.SetStringKey("name", "Cellular1_Provider");
    home_provider.SetStringKey("code", "000000");
    home_provider.SetStringKey("country", "us");
    SetDeviceProperty(kCellularDevicePath, shill::kHomeProviderProperty,
                      home_provider);
    SetDeviceProperty(kCellularDevicePath, shill::kTechnologyFamilyProperty,
                      base::Value(shill::kNetworkTechnologyGsm));
    SetDeviceProperty(kCellularDevicePath, shill::kMeidProperty,
                      base::Value("test_meid"));
    SetDeviceProperty(kCellularDevicePath, shill::kImeiProperty,
                      base::Value("test_imei"));
    SetDeviceProperty(kCellularDevicePath, shill::kIccidProperty,
                      base::Value("test_iccid"));
    SetDeviceProperty(kCellularDevicePath, shill::kEsnProperty,
                      base::Value("test_esn"));
    SetDeviceProperty(kCellularDevicePath, shill::kMdnProperty,
                      base::Value("test_mdn"));
    SetDeviceProperty(kCellularDevicePath, shill::kMinProperty,
                      base::Value("test_min"));
    SetDeviceProperty(kCellularDevicePath, shill::kModelIdProperty,
                      base::Value("test_model_id"));
    device_test()->SetSimLocked(kCellularDevicePath, false);

    // Add the Cellular Service.
    AddService(kCellular1ServicePath, "cellular1", shill::kTypeCellular,
               shill::kStateIdle);
    service_test()->SetServiceProperty(kCellular1ServicePath,
                                       shill::kCellularAllowRoamingProperty,
                                       base::Value(false));
    service_test()->SetServiceProperty(
        kCellular1ServicePath, shill::kAutoConnectProperty, base::Value(true));
    service_test()->SetServiceProperty(kCellular1ServicePath,
                                       shill::kIccidProperty,
                                       base::Value("test_iccid"));
    service_test()->SetServiceProperty(
        kCellular1ServicePath, shill::kNetworkTechnologyProperty,
        base::Value(shill::kNetworkTechnologyGsm));
    service_test()->SetServiceProperty(
        kCellular1ServicePath, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateNotActivated));
    service_test()->SetServiceProperty(kCellular1ServicePath,
                                       shill::kRoamingStateProperty,
                                       base::Value(shill::kRoamingStateHome));

    profile_test()->AddService(kUser1ProfilePath, kCellular1ServicePath);
    content::RunAllPendingInMessageLoop();
  }

  void SetupTether() {
    chromeos::NetworkStateHandler* network_state_handler =
        chromeos::NetworkHandler::Get()->network_state_handler();
    network_state_handler->SetTetherTechnologyState(
        chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
    network_state_handler->AddTetherNetworkState(
        "tetherGuid1", "tetherName1", "tetherCarrier1",
        50 /* battery_percentage */, 75 /* signal_strength */,
        true /* has_connected_to_host */);
    network_state_handler->AddTetherNetworkState(
        "tetherGuid2", "tetherName2", "tetherCarrier2",
        75 /* battery_percentage */, 100 /* signal_strength */,
        false /* has_connected_to_host */);
  }

  void AddService(const std::string& service_path,
                  const std::string& name,
                  const std::string& type,
                  const std::string& state) {
    service_test()->AddService(service_path, service_path + "_guid", name, type,
                               state, true /* add_to_visible */);
  }

  void SetDeviceProperty(const std::string& device_path,
                         const std::string& name,
                         const base::Value& value) {
    device_test()->SetDeviceProperty(device_path, name, value,
                                     /*notify_changed=*/true);
  }

  static std::unique_ptr<KeyedService> CreateNetworkingPrivateDelegate(
      content::BrowserContext* context) {
    std::unique_ptr<NetworkingPrivateDelegate> result(
        new NetworkingPrivateChromeOS(context));
    std::unique_ptr<NetworkingPrivateDelegate::UIDelegate> ui_delegate(
        new UIDelegateStub);
    result->set_ui_delegate(std::move(ui_delegate));
    return result;
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();

    NetworkingPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&CreateNetworkingPrivateDelegate));

    InitializeSanitizedUsername();

    network_handler_test_helper_ =
        std::make_unique<chromeos::NetworkHandlerTestHelper>();
    device_test()->ClearDevices();
    service_test()->ClearServices();

    // Sends a notification about the added profile.
    profile_test()->AddProfile(kUser1ProfilePath, userhash_);

    // Add IPConfigs
    base::DictionaryValue ipconfig;
    ipconfig.SetKey(shill::kAddressProperty, base::Value("0.0.0.0"));
    ipconfig.SetKey(shill::kGatewayProperty, base::Value("0.0.0.1"));
    ipconfig.SetKey(shill::kPrefixlenProperty, base::Value(0));
    ipconfig.SetKey(shill::kMethodProperty, base::Value(shill::kTypeIPv4));
    network_handler_test_helper_->ip_config_test()->AddIPConfig(kIPConfigPath,
                                                                ipconfig);

    // Add Devices
    device_test()->AddDevice(kEthernetDevicePath, shill::kTypeEthernet,
                             "stub_ethernet_device1");

    device_test()->AddDevice(kWifiDevicePath, shill::kTypeWifi,
                             "stub_wifi_device1");
    base::ListValue wifi_ip_configs;
    wifi_ip_configs.Append(kIPConfigPath);
    SetDeviceProperty(kWifiDevicePath, shill::kIPConfigsProperty,
                      wifi_ip_configs);
    SetDeviceProperty(kWifiDevicePath, shill::kAddressProperty,
                      base::Value("001122aabbcc"));

    // Add Services
    AddService("stub_ethernet", "eth0", shill::kTypeEthernet,
               shill::kStateOnline);
    service_test()->SetServiceProperty(
        "stub_ethernet", shill::kProfileProperty,
        base::Value(ShillProfileClient::GetSharedProfilePath()));
    profile_test()->AddService(ShillProfileClient::GetSharedProfilePath(),
                               "stub_ethernet");

    AddService(kWifi1ServicePath, "wifi1", shill::kTypeWifi,
               shill::kStateOnline);
    service_test()->SetServiceProperty(kWifi1ServicePath,
                                       shill::kSecurityClassProperty,
                                       base::Value(shill::kSecurityWep));
    service_test()->SetServiceProperty(kWifi1ServicePath, shill::kWifiBSsid,
                                       base::Value("00:01:02:03:04:05"));
    service_test()->SetServiceProperty(
        kWifi1ServicePath, shill::kSignalStrengthProperty, base::Value(40));
    service_test()->SetServiceProperty(kWifi1ServicePath,
                                       shill::kProfileProperty,
                                       base::Value(kUser1ProfilePath));
    service_test()->SetServiceProperty(
        kWifi1ServicePath, shill::kConnectableProperty, base::Value(true));
    service_test()->SetServiceProperty(kWifi1ServicePath,
                                       shill::kDeviceProperty,
                                       base::Value(kWifiDevicePath));
    base::DictionaryValue static_ipconfig;
    static_ipconfig.SetKey(shill::kAddressProperty, base::Value("1.2.3.4"));
    static_ipconfig.SetKey(shill::kGatewayProperty, base::Value("0.0.0.0"));
    static_ipconfig.SetKey(shill::kPrefixlenProperty, base::Value(1));
    service_test()->SetServiceProperty(
        kWifi1ServicePath, shill::kStaticIPConfigProperty, static_ipconfig);
    base::ListValue frequencies1;
    frequencies1.Append(2400);
    service_test()->SetServiceProperty(
        kWifi1ServicePath, shill::kWifiFrequencyListProperty, frequencies1);
    service_test()->SetServiceProperty(kWifi1ServicePath, shill::kWifiFrequency,
                                       base::Value(2400));
    profile_test()->AddService(kUser1ProfilePath, kWifi1ServicePath);

    AddService(kWifi2ServicePath, "wifi2_PSK", shill::kTypeWifi,
               shill::kStateIdle);
    service_test()->SetServiceProperty(kWifi2ServicePath,
                                       shill::kSecurityClassProperty,
                                       base::Value(shill::kSecurityPsk));
    service_test()->SetServiceProperty(
        kWifi2ServicePath, shill::kSignalStrengthProperty, base::Value(80));
    service_test()->SetServiceProperty(
        kWifi2ServicePath, shill::kConnectableProperty, base::Value(true));

    base::ListValue frequencies2;
    frequencies2.Append(2400);
    frequencies2.Append(5000);
    service_test()->SetServiceProperty(
        kWifi2ServicePath, shill::kWifiFrequencyListProperty, frequencies2);
    service_test()->SetServiceProperty(kWifi2ServicePath, shill::kWifiFrequency,
                                       base::Value(5000));
    service_test()->SetServiceProperty(kWifi2ServicePath,
                                       shill::kProfileProperty,
                                       base::Value(kUser1ProfilePath));
    profile_test()->AddService(kUser1ProfilePath, kWifi2ServicePath);

    AddService("stub_vpn1", "vpn1", shill::kTypeVPN, shill::kStateOnline);
    service_test()->SetServiceProperty("stub_vpn1",
                                       shill::kProviderTypeProperty,
                                       base::Value(shill::kProviderOpenVpn));
    profile_test()->AddService(kUser1ProfilePath, "stub_vpn1");

    AddService("stub_vpn2", "vpn2", shill::kTypeVPN, shill::kStateOffline);
    service_test()->SetServiceProperty(
        "stub_vpn2", shill::kProviderTypeProperty,
        base::Value(shill::kProviderThirdPartyVpn));
    service_test()->SetServiceProperty(
        "stub_vpn2", shill::kProviderHostProperty,
        base::Value("third_party_provider_extension_id"));
    profile_test()->AddService(kUser1ProfilePath, "stub_vpn2");

    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    network_handler_test_helper_->RegisterPrefs(user_prefs_.registry(),
                                                local_state_.registry());

    network_handler_test_helper_->InitializePrefs(&user_prefs_, &local_state_);

    content::RunAllPendingInMessageLoop();
  }

  void TearDownOnMainThread() { network_handler_test_helper_.reset(); }

  ShillServiceClient::TestInterface* service_test() {
    return network_handler_test_helper_->service_test();
  }
  ShillProfileClient::TestInterface* profile_test() {
    return network_handler_test_helper_->profile_test();
  }
  ShillDeviceClient::TestInterface* device_test() {
    return network_handler_test_helper_->device_test();
  }
  ShillManagerClient::TestInterface* manager_test() {
    return network_handler_test_helper_->manager_test();
  }

 protected:
  std::unique_ptr<chromeos::NetworkHandlerTestHelper>
      network_handler_test_helper_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  std::string userhash_;
};

// Place each subtest into a separate browser test so that the stub networking
// library state is reset for each subtest run. This way they won't affect each
// other.

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, StartConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, StartDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, StartActivate) {
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("startActivate")) << message_;
  EXPECT_EQ(1, UIDelegateStub::s_show_account_details_called_);
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       StartConnectNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       StartDisconnectNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       StartGetPropertiesNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startGetPropertiesNonexistent"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetNetworks) {
  // Hide stub_wifi2.
  service_test()->SetServiceProperty(kWifi2ServicePath, shill::kVisibleProperty,
                                     base::Value(false));
  // Add a couple of additional networks that are not configured (saved).
  AddService("stub_wifi3", "wifi3", shill::kTypeWifi, shill::kStateIdle);
  AddService("stub_wifi4", "wifi4", shill::kTypeWifi, shill::kStateIdle);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(RunNetworkingSubtest("getNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetVisibleNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       GetVisibleNetworksWifi) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworksWifi")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, EnabledNetworkTypes) {
  EXPECT_TRUE(RunNetworkingSubtest("enabledNetworkTypesDisable")) << message_;
  EXPECT_TRUE(RunNetworkingSubtest("enabledNetworkTypesEnable")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetDeviceStates) {
  SetupCellular();
  manager_test()->RemoveTechnology("cellular");
  manager_test()->AddTechnology("cellular", false /* disabled */);
  manager_test()->SetTechnologyInitializing("cellular", true);
  EXPECT_TRUE(RunNetworkingSubtest("getDeviceStates")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, RequestNetworkScan) {
  EXPECT_TRUE(RunNetworkingSubtest("requestNetworkScan")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       RequestNetworkScanCellular) {
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("requestNetworkScanCellular")) << message_;
}

// Properties are filtered and translated through
// ShillToONCTranslator::TranslateWiFiWithState
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("getProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       GetCellularProperties) {
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("getPropertiesCellular")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetState) {
  EXPECT_TRUE(RunNetworkingSubtest("getState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetStateNonExistent) {
  EXPECT_TRUE(RunNetworkingSubtest("getStateNonExistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       SetCellularProperties) {
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("setCellularProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, SetWiFiProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("setWiFiProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, SetVPNProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("setVPNProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, CreateNetwork) {
  EXPECT_TRUE(RunNetworkingSubtest("createNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       CreateNetworkForPolicyControlledNetwork) {
  constexpr char kUserPolicyBlob[] =
      R"({
           "NetworkConfigurations": [{
             "GUID": "stub_wifi2",
             "Type": "WiFi",
             "Name": "My WiFi Network",
             "WiFi": {
               "HexSSID": "77696669325F50534B",
               "Passphrase": "passphrase",
               "Recommended": [ "AutoConnect", "Passphrase" ],
               "Security": "WPA-PSK"
             }
           }],
           "Certificates": [],
           "Type": "UnencryptedConfiguration"
         })";

  policy::PolicyMap policy;
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(kUserPolicyBlob),
             nullptr);
  provider_.UpdateChromePolicy(policy);

  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(RunNetworkingSubtest("createNetworkForPolicyControlledNetwork"));
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, ForgetNetwork) {
  EXPECT_TRUE(RunNetworkingSubtest("forgetNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       ForgetPolicyControlledNetwork) {
  constexpr char kUserPolicyBlob[] =
      R"({
           "NetworkConfigurations": [{
             "GUID": "stub_wifi2",
             "Type": "WiFi",
             "Name": "My WiFi Network",
             "WiFi": {
               "HexSSID": "77696669325F50534B",
               "Passphrase": "passphrase",
               "Recommended": [ "AutoConnect", "Passphrase" ],
               "Security": "WPA-PSK"
             }
           }],
           "Certificates": [],
           "Type": "UnencryptedConfiguration"
         })";

  policy::PolicyMap policy;
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(kUserPolicyBlob),
             nullptr);
  provider_.UpdateChromePolicy(policy);

  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(RunNetworkingSubtest("forgetPolicyControlledNetwork"));
}

// TODO(stevenjb): Find a better way to set this up on Chrome OS.
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetManagedProperties) {
  constexpr char kUidataBlob[] =
      R"({
        "user_settings": {
          "WiFi": {"Passphrase": "FAKE_CREDENTIAL_VPaJDV9x"}
        }
      })";
  service_test()->SetServiceProperty(kWifi2ServicePath, shill::kUIDataProperty,
                                     base::Value(kUidataBlob));
  service_test()->SetServiceProperty(
      kWifi2ServicePath, shill::kAutoConnectProperty, base::Value(false));

  // Update the profile entry.
  profile_test()->AddService(kUser1ProfilePath, kWifi2ServicePath);

  content::RunAllPendingInMessageLoop();

  constexpr char kUserPolicyBlob[] = R"({
      "NetworkConfigurations": [
          { "GUID": "stub_wifi2",
            "Type": "WiFi",
            "Name": "My WiFi Network",
            "ProxySettings":{
                "Type": "Direct"
            },
            "WiFi": {
              "HexSSID": "77696669325F50534B",
              "Passphrase": "passphrase",
              "Recommended": [ "AutoConnect", "Passphrase" ],
              "Security": "WPA-PSK" }
          }
        ],
        "Certificates": [],
        "Type": "UnencryptedConfiguration"
      })";

  policy::PolicyMap policy;
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(kUserPolicyBlob),
             nullptr);
  provider_.UpdateChromePolicy(policy);

  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(RunNetworkingSubtest("getManagedProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetErrorState) {
  chromeos::NetworkHandler::Get()->network_state_handler()->SetErrorForTest(
      kWifi1ServicePath, "TestErrorState");
  EXPECT_TRUE(RunNetworkingSubtest("getErrorState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnNetworksChangedEventConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventConnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnNetworksChangedEventDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventDisconnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnNetworkListChangedEvent) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworkListChangedEvent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnDeviceStateListChangedEvent) {
  EXPECT_TRUE(RunNetworkingSubtest("onDeviceStateListChangedEvent"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnDeviceScanningChangedEvent) {
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("onDeviceScanningChangedEvent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnCertificateListsChangedEvent) {
  ExtensionTestMessageListener listener("eventListenerReady", false);
  listener.SetOnSatisfied(base::BindOnce([](const std::string& message) {
    chromeos::NetworkHandler::Get()
        ->network_certificate_handler()
        ->AddAuthorityCertificateForTest("authority_cert");
  }));
  EXPECT_TRUE(RunNetworkingSubtest("onCertificateListsChangedEvent"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       GetCaptivePortalStatus) {
  // Ethernet defaults to online. Set wifi1 to idle -> 'Offline', and wifi2 to
  // redirect-found -> 'Portal'.
  service_test()->SetServiceProperty(kWifi1ServicePath, shill::kStateProperty,
                                     base::Value(shill::kStateIdle));
  service_test()->SetServiceProperty(kWifi2ServicePath, shill::kStateProperty,
                                     base::Value(shill::kStateRedirectFound));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(RunNetworkingSubtest("getCaptivePortalStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       CaptivePortalNotification) {
  // Make wifi1 the default service since captive portal notifications only
  // occur for the default service.
  service_test()->RemoveService("stub_ethernet");
  service_test()->RemoveService("stub_vpn1");

  ExtensionTestMessageListener listener("notifyPortalDetectorObservers", false);
  listener.SetOnSatisfied(
      base::BindLambdaForTesting([&](const std::string& message) {
        service_test()->SetServiceProperty(
            kWifi1ServicePath, shill::kStateProperty,
            base::Value(shill::kStateRedirectFound));
      }));

  EXPECT_TRUE(RunNetworkingSubtest("captivePortalNotification")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, UnlockCellularSim) {
  SetupCellular();
  // Lock the SIM
  device_test()->SetSimLocked(kCellularDevicePath, true);
  EXPECT_TRUE(RunNetworkingSubtest("unlockCellularSim")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, SetCellularSimState) {
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("setCellularSimState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       SelectCellularMobileNetwork) {
  SetupCellular();
  // Create fake list of found networks.
  std::unique_ptr<base::ListValue> found_networks =
      extensions::ListBuilder()
          .Append(extensions::DictionaryBuilder()
                      .Set(shill::kNetworkIdProperty, "network1")
                      .Set(shill::kTechnologyProperty, "GSM")
                      .Set(shill::kStatusProperty, "current")
                      .Build())
          .Append(extensions::DictionaryBuilder()
                      .Set(shill::kNetworkIdProperty, "network2")
                      .Set(shill::kTechnologyProperty, "GSM")
                      .Set(shill::kStatusProperty, "available")
                      .Build())
          .Build();
  SetDeviceProperty(kCellularDevicePath, shill::kFoundNetworksProperty,
                    *found_networks);
  EXPECT_TRUE(RunNetworkingSubtest("selectCellularMobileNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, CellularSimPuk) {
  SetupCellular();
  // Lock the SIM
  device_test()->SetSimLocked(kCellularDevicePath, true);
  EXPECT_TRUE(RunNetworkingSubtest("cellularSimPuk")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetGlobalPolicy) {
  base::DictionaryValue global_config;
  global_config.SetKey(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      base::Value(true));
  global_config.SetKey(
      ::onc::global_network_config::kAllowOnlyPolicyWiFiToConnect,
      base::Value(false));
  global_config.SetKey("SomeNewGlobalPolicy", base::Value(false));
  chromeos::NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                  std::string() /* no username hash */, base::ListValue(),
                  global_config);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(RunNetworkingSubtest("getGlobalPolicy")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       Tether_GetTetherNetworks) {
  SetupTether();
  EXPECT_TRUE(RunNetworkingSubtest("getTetherNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       Tether_GetTetherNetworkProperties) {
  SetupTether();
  EXPECT_TRUE(RunNetworkingSubtest("getTetherNetworkProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       Tether_GetTetherNetworkManagedProperties) {
  SetupTether();
  EXPECT_TRUE(RunNetworkingSubtest("getTetherNetworkManagedProperties"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       Tether_GetTetherNetworkState) {
  SetupTether();
  EXPECT_TRUE(RunNetworkingSubtest("getTetherNetworkState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetCertificateLists) {
  chromeos::NetworkHandler::Get()
      ->network_certificate_handler()
      ->AddAuthorityCertificateForTest("authority_cert");
  EXPECT_TRUE(RunNetworkingSubtest("getCertificateLists")) << message_;
}

// Tests subset of networking API for the networking API alias - to verify that
// using API methods and event does not cause access exceptions (due to
// missing permissions).
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, Alias) {
  SetupCellular();
  EXPECT_TRUE(RunExtensionTest("networking_private/alias",
                               {.launch_as_platform_app = true}))
      << message_;
}

}  // namespace
