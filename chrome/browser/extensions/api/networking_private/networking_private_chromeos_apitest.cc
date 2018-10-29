// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/extensions/api/networking_cast_private/chrome_networking_cast_private_delegate.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_credentials_getter.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_ui_delegate_chromeos.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_certificate_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "dbus/object_path.h"
#include "extensions/browser/api/networking_private/networking_private_chromeos.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/switches.h"
#include "extensions/common/value_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

// This tests the Chrome OS implementation of the networkingPrivate API
// (NetworkingPrivateChromeOS). Note: The test expectations for chromeos, and
// win/mac (NetworkingPrivateServiceClient) are different to reflect the
// different implementations, but should be kept similar where possible.

using testing::Return;
using testing::_;

using chromeos::CryptohomeClient;
using chromeos::DBusThreadManager;
using chromeos::NetworkPortalDetector;
using chromeos::NetworkPortalDetectorTestImpl;
using chromeos::ShillDeviceClient;
using chromeos::ShillIPConfigClient;
using chromeos::ShillManagerClient;
using chromeos::ShillProfileClient;
using chromeos::ShillServiceClient;

using extensions::ChromeNetworkingCastPrivateDelegate;
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

// Stub Verify* methods implementation to satisfy expectations of
// networking_private_apitest.
class TestNetworkingCastPrivateDelegate
    : public ChromeNetworkingCastPrivateDelegate {
 public:
  TestNetworkingCastPrivateDelegate() = default;
  ~TestNetworkingCastPrivateDelegate() override = default;

  // VerifyDelegate
  void VerifyDestination(std::unique_ptr<Credentials> credentials,
                         const VerifiedCallback& success_callback,
                         const FailureCallback& failure_callback) override {
    AssertCredentials(*credentials);
    success_callback.Run(true);
  }

  void VerifyAndEncryptCredentials(
      const std::string& guid,
      std::unique_ptr<Credentials> credentials,
      const DataCallback& success_callback,
      const FailureCallback& failure_callback) override {
    AssertCredentials(*credentials);
    success_callback.Run("encrypted_credentials");
  }

  void VerifyAndEncryptData(const std::string& data,
                            std::unique_ptr<Credentials> credentials,
                            const DataCallback& success_callback,
                            const FailureCallback& failure_callback) override {
    AssertCredentials(*credentials);
    success_callback.Run("encrypted_data");
  }

 private:
  void AssertCredentials(const Credentials& credentials) {
    ASSERT_EQ("certificate", credentials.certificate());
    ASSERT_EQ("ica1,ica2,ica3",
              base::JoinString(credentials.intermediate_certificates(), ","));
    ASSERT_EQ("cHVibGljX2tleQ==", credentials.public_key());
    ASSERT_EQ("00:01:02:03:04:05", credentials.device_bssid());
    ASSERT_EQ("c2lnbmVkX2RhdGE=", credentials.signed_data());
    ASSERT_EQ(
        "Device 0123,device_serial,00:01:02:03:04:05,cHVibGljX2tleQ==,nonce",
        credentials.unsigned_data());
  }

  DISALLOW_COPY_AND_ASSIGN(TestNetworkingCastPrivateDelegate);
};

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

class TestListener : public content::NotificationObserver {
 public:
  TestListener(const std::string& message, const base::Closure& callback)
      : message_(message), callback_(callback) {
    registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
  }

  void Observe(int type,
               const content::NotificationSource& /* source */,
               const content::NotificationDetails& details) override {
    const std::string& message =
        content::Details<std::pair<std::string, bool*>>(details).ptr()->first;
    if (message == message_)
      callback_.Run();
  }

 private:
  std::string message_;
  base::Closure callback_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TestListener);
};

class NetworkingPrivateChromeOSApiTest : public extensions::ExtensionApiTest {
 public:
  NetworkingPrivateChromeOSApiTest()
      : detector_(nullptr),
        manager_test_(nullptr),
        profile_test_(nullptr),
        service_test_(nullptr),
        device_test_(nullptr) {}

  bool RunNetworkingSubtest(const std::string& test) {
    const std::string arg =
        base::StringPrintf("{\"test\": \"%s\"}", test.c_str());
    return RunPlatformAppTestWithArg("networking_private/chromeos",
                                     arg.c_str());
  }

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    // Whitelist the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "epcifkihnkjgphfkloaaleeakhpmgdmn");

    // TODO(pneubeck): Remove the following hack, once the NetworkingPrivateAPI
    // uses the ProfileHelper to obtain the userhash crbug/238623.
    cryptohome::AccountIdentifier login_user;
    login_user.set_account_id(user_manager::CanonicalizeUserID(
        command_line->GetSwitchValueNative(chromeos::switches::kLoginUser)));
    const std::string sanitized_user =
        CryptohomeClient::GetStubSanitizedUsername(login_user);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    sanitized_user);
  }

  void InitializeSanitizedUsername() {
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    user_manager::User* user = user_manager->GetActiveUser();
    CHECK(user);
    std::string userhash;
    DBusThreadManager::Get()->GetCryptohomeClient()->GetSanitizedUsername(
        cryptohome::CreateAccountIdentifierFromAccountId(user->GetAccountId()),
        base::BindOnce(
            [](std::string* out, base::Optional<std::string> result) {
              CHECK(result.has_value());
              *out = std::move(result).value();
            },
            &userhash_));
    content::RunAllPendingInMessageLoop();
    CHECK(!userhash_.empty());
  }

  void SetupCellular() {
    UIDelegateStub::s_show_account_details_called_ = 0;

    // Add a Cellular GSM Device.
    device_test_->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                            "stub_cellular_device1");
    SetDeviceProperty(kCellularDevicePath, shill::kCarrierProperty,
                      base::Value("Cellular1_Carrier"));
    base::DictionaryValue home_provider;
    home_provider.SetString("name", "Cellular1_Provider");
    home_provider.SetString("code", "000000");
    home_provider.SetString("country", "us");
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
    device_test_->SetSimLocked(kCellularDevicePath, false);

    // Add the Cellular Service.
    AddService(kCellular1ServicePath, "cellular1", shill::kTypeCellular,
               shill::kStateIdle);
    service_test_->SetServiceProperty(
        kCellular1ServicePath, shill::kAutoConnectProperty, base::Value(true));
    service_test_->SetServiceProperty(
        kCellular1ServicePath, shill::kNetworkTechnologyProperty,
        base::Value(shill::kNetworkTechnologyGsm));
    service_test_->SetServiceProperty(
        kCellular1ServicePath, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateNotActivated));
    service_test_->SetServiceProperty(kCellular1ServicePath,
                                      shill::kRoamingStateProperty,
                                      base::Value(shill::kRoamingStateHome));

    profile_test_->AddService(kUser1ProfilePath, kCellular1ServicePath);
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
    service_test_->AddService(service_path, service_path + "_guid", name, type,
                              state, true /* add_to_visible */);
  }

  void SetDeviceProperty(const std::string& device_path,
                         const std::string& name,
                         const base::Value& value) {
    device_test_->SetDeviceProperty(device_path, name, value,
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

  void SetUp() override {
    networking_cast_delegate_factory_ = base::Bind(
        &NetworkingPrivateChromeOSApiTest::CreateNetworkingCastPrivateDelegate,
        base::Unretained(this));
    ChromeNetworkingCastPrivateDelegate::SetFactoryCallbackForTest(
        &networking_cast_delegate_factory_);

    extensions::ExtensionApiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    detector_ = new NetworkPortalDetectorTestImpl();
    chromeos::network_portal_detector::InitializeForTesting(detector_);

    extensions::ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();

    NetworkingPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&CreateNetworkingPrivateDelegate));

    InitializeSanitizedUsername();

    DBusThreadManager* dbus_manager = DBusThreadManager::Get();
    manager_test_ = dbus_manager->GetShillManagerClient()->GetTestInterface();
    profile_test_ = dbus_manager->GetShillProfileClient()->GetTestInterface();
    service_test_ = dbus_manager->GetShillServiceClient()->GetTestInterface();
    device_test_ = dbus_manager->GetShillDeviceClient()->GetTestInterface();

    ShillIPConfigClient::TestInterface* ip_config_test =
        dbus_manager->GetShillIPConfigClient()->GetTestInterface();

    device_test_->ClearDevices();
    service_test_->ClearServices();

    // Sends a notification about the added profile.
    profile_test_->AddProfile(kUser1ProfilePath, userhash_);

    // Enable technologies.
    manager_test_->AddTechnology("wimax", true);

    // Add IPConfigs
    base::DictionaryValue ipconfig;
    ipconfig.SetKey(shill::kAddressProperty, base::Value("0.0.0.0"));
    ipconfig.SetKey(shill::kGatewayProperty, base::Value("0.0.0.1"));
    ipconfig.SetKey(shill::kPrefixlenProperty, base::Value(0));
    ipconfig.SetKey(shill::kMethodProperty, base::Value(shill::kTypeIPv4));
    ip_config_test->AddIPConfig(kIPConfigPath, ipconfig);

    // Add Devices
    device_test_->AddDevice(kEthernetDevicePath, shill::kTypeEthernet,
                            "stub_ethernet_device1");

    device_test_->AddDevice(kWifiDevicePath, shill::kTypeWifi,
                            "stub_wifi_device1");
    base::ListValue wifi_ip_configs;
    wifi_ip_configs.AppendString(kIPConfigPath);
    SetDeviceProperty(kWifiDevicePath, shill::kIPConfigsProperty,
                      wifi_ip_configs);
    SetDeviceProperty(kWifiDevicePath, shill::kAddressProperty,
                      base::Value("001122aabbcc"));

    // Add Services
    AddService("stub_ethernet", "eth0", shill::kTypeEthernet,
               shill::kStateOnline);
    service_test_->SetServiceProperty(
        "stub_ethernet", shill::kProfileProperty,
        base::Value(ShillProfileClient::GetSharedProfilePath()));
    profile_test_->AddService(ShillProfileClient::GetSharedProfilePath(),
                              "stub_ethernet");

    AddService(kWifi1ServicePath, "wifi1", shill::kTypeWifi,
               shill::kStateOnline);
    service_test_->SetServiceProperty(kWifi1ServicePath,
                                      shill::kSecurityClassProperty,
                                      base::Value(shill::kSecurityWep));
    service_test_->SetServiceProperty(kWifi1ServicePath, shill::kWifiBSsid,
                                      base::Value("00:01:02:03:04:05"));
    service_test_->SetServiceProperty(
        kWifi1ServicePath, shill::kSignalStrengthProperty, base::Value(40));
    service_test_->SetServiceProperty(kWifi1ServicePath,
                                      shill::kProfileProperty,
                                      base::Value(kUser1ProfilePath));
    service_test_->SetServiceProperty(
        kWifi1ServicePath, shill::kConnectableProperty, base::Value(true));
    service_test_->SetServiceProperty(kWifi1ServicePath, shill::kDeviceProperty,
                                      base::Value(kWifiDevicePath));
    service_test_->SetServiceProperty(
        kWifi1ServicePath, shill::kTetheringProperty,
        base::Value(shill::kTetheringNotDetectedState));
    base::DictionaryValue static_ipconfig;
    static_ipconfig.SetKey(shill::kAddressProperty, base::Value("1.2.3.4"));
    service_test_->SetServiceProperty(
        kWifi1ServicePath, shill::kStaticIPConfigProperty, static_ipconfig);
    base::ListValue frequencies1;
    frequencies1.AppendInteger(2400);
    service_test_->SetServiceProperty(
        kWifi1ServicePath, shill::kWifiFrequencyListProperty, frequencies1);
    service_test_->SetServiceProperty(kWifi1ServicePath, shill::kWifiFrequency,
                                      base::Value(2400));
    profile_test_->AddService(kUser1ProfilePath, kWifi1ServicePath);

    AddService(kWifi2ServicePath, "wifi2_PSK", shill::kTypeWifi,
               shill::kStateIdle);
    service_test_->SetServiceProperty(kWifi2ServicePath,
                                      shill::kSecurityClassProperty,
                                      base::Value(shill::kSecurityPsk));
    service_test_->SetServiceProperty(
        kWifi2ServicePath, shill::kSignalStrengthProperty, base::Value(80));
    service_test_->SetServiceProperty(
        kWifi2ServicePath, shill::kConnectableProperty, base::Value(true));
    service_test_->SetServiceProperty(
        kWifi2ServicePath, shill::kTetheringProperty,
        base::Value(shill::kTetheringNotDetectedState));

    AddService("stub_wimax", "wimax", shill::kTypeWimax, shill::kStateOnline);
    service_test_->SetServiceProperty(
        "stub_wimax", shill::kSignalStrengthProperty, base::Value(40));
    service_test_->SetServiceProperty("stub_wimax", shill::kProfileProperty,
                                      base::Value(kUser1ProfilePath));
    service_test_->SetServiceProperty("stub_wimax", shill::kConnectableProperty,
                                      base::Value(true));
    profile_test_->AddService(kUser1ProfilePath, "stub_wimax");

    base::ListValue frequencies2;
    frequencies2.AppendInteger(2400);
    frequencies2.AppendInteger(5000);
    service_test_->SetServiceProperty(
        kWifi2ServicePath, shill::kWifiFrequencyListProperty, frequencies2);
    service_test_->SetServiceProperty(kWifi2ServicePath, shill::kWifiFrequency,
                                      base::Value(5000));
    service_test_->SetServiceProperty(kWifi2ServicePath,
                                      shill::kProfileProperty,
                                      base::Value(kUser1ProfilePath));
    profile_test_->AddService(kUser1ProfilePath, kWifi2ServicePath);

    AddService("stub_vpn1", "vpn1", shill::kTypeVPN, shill::kStateOnline);
    service_test_->SetServiceProperty("stub_vpn1", shill::kProviderTypeProperty,
                                      base::Value(shill::kProviderOpenVpn));
    profile_test_->AddService(kUser1ProfilePath, "stub_vpn1");

    AddService("stub_vpn2", "vpn2", shill::kTypeVPN, shill::kStateOffline);
    service_test_->SetServiceProperty(
        "stub_vpn2", shill::kProviderTypeProperty,
        base::Value(shill::kProviderThirdPartyVpn));
    service_test_->SetServiceProperty(
        "stub_vpn2", shill::kProviderHostProperty,
        base::Value("third_party_provider_extension_id"));
    profile_test_->AddService(kUser1ProfilePath, "stub_vpn2");

    content::RunAllPendingInMessageLoop();
  }

  void TearDown() override {
    extensions::ExtensionApiTest::TearDown();
    ChromeNetworkingCastPrivateDelegate::SetFactoryCallbackForTest(nullptr);
  }

  std::unique_ptr<ChromeNetworkingCastPrivateDelegate>
  CreateNetworkingCastPrivateDelegate() {
    return std::make_unique<TestNetworkingCastPrivateDelegate>();
  }

  bool SetupCertificates() {
    net::ScopedCERTCertificateList cert_list =
        net::CreateCERTCertificateListFromFile(
            net::GetTestCertsDirectory(), "client_1_ca.pem",
            net::X509Certificate::FORMAT_AUTO);
    if (cert_list.empty())
      return false;
    // TODO(stevenjb): Figure out a simple way to import a test user cert.

    chromeos::NetworkHandler::Get()
        ->network_certificate_handler()
        ->SetCertificatesForTest(cert_list);
    return true;
  }

 protected:
  NetworkPortalDetectorTestImpl* detector() { return detector_; }

  NetworkPortalDetectorTestImpl* detector_;
  ShillManagerClient::TestInterface* manager_test_;
  ShillProfileClient::TestInterface* profile_test_;
  ShillServiceClient::TestInterface* service_test_;
  ShillDeviceClient::TestInterface* device_test_;
  policy::MockConfigurationPolicyProvider provider_;
  std::string userhash_;

 private:
  ChromeNetworkingCastPrivateDelegate::FactoryCallback
      networking_cast_delegate_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateChromeOSApiTest);
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

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, StartActivateSprint) {
  SetupCellular();
  // Set the carrier to Sprint.
  DBusThreadManager::Get()->GetShillDeviceClient()->SetCarrier(
      dbus::ObjectPath(kCellularDevicePath), shill::kCarrierSprint,
      base::DoNothing(),
      base::BindRepeating([](const std::string&, const std::string&) {}));
  EXPECT_TRUE(RunNetworkingSubtest("startActivateSprint")) << message_;
  EXPECT_EQ(0, UIDelegateStub::s_show_account_details_called_);
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
  service_test_->SetServiceProperty(kWifi2ServicePath, shill::kVisibleProperty,
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
  EXPECT_TRUE(RunNetworkingSubtest("enabledNetworkTypes")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetDeviceStates) {
  SetupCellular();
  manager_test_->RemoveTechnology("cellular");
  manager_test_->AddTechnology("cellular", false /* disabled */);
  manager_test_->SetTechnologyInitializing("cellular", true);
  manager_test_->RemoveTechnology("wimax");
  manager_test_->AddTechnology("wimax", false /* disabled */);
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

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       GetCellularPropertiesDefault) {
  SetupCellular();
  const chromeos::NetworkState* cellular =
      chromeos::NetworkHandler::Get()
          ->network_state_handler()
          ->FirstNetworkByType(chromeos::NetworkTypePattern::Cellular());
  ASSERT_TRUE(cellular);
  std::string cellular_guid = std::string(kCellular1ServicePath) + "_guid";
  EXPECT_EQ(cellular_guid, cellular->guid());
  // Remove the Cellular service. This should create a default Cellular network.
  service_test_->RemoveService(kCellular1ServicePath);
  content::RunAllPendingInMessageLoop();
  cellular = chromeos::NetworkHandler::Get()
                 ->network_state_handler()
                 ->FirstNetworkByType(chromeos::NetworkTypePattern::Cellular());
  ASSERT_TRUE(cellular);
  EXPECT_EQ(cellular_guid, cellular->guid());
  EXPECT_TRUE(RunNetworkingSubtest("getPropertiesCellularDefault")) << message_;
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
  const std::string user_policy_blob =
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
             policy::POLICY_SOURCE_CLOUD,
             base::WrapUnique(new base::Value(user_policy_blob)), nullptr);
  provider_.UpdateChromePolicy(policy);

  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(RunNetworkingSubtest("createNetworkForPolicyControlledNetwork"));
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, ForgetNetwork) {
  EXPECT_TRUE(RunNetworkingSubtest("forgetNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       ForgetPolicyControlledNetwork) {
  const std::string user_policy_blob =
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
             policy::POLICY_SOURCE_CLOUD,
             base::WrapUnique(new base::Value(user_policy_blob)), nullptr);
  provider_.UpdateChromePolicy(policy);

  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(RunNetworkingSubtest("forgetPolicyControlledNetwork"));
}

// TODO(stevenjb): Find a better way to set this up on Chrome OS.
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetManagedProperties) {
  const std::string uidata_blob =
      "{ \"user_settings\": {"
      "      \"WiFi\": {"
      "        \"Passphrase\": \"FAKE_CREDENTIAL_VPaJDV9x\" }"
      "    }"
      "}";
  service_test_->SetServiceProperty(kWifi2ServicePath, shill::kUIDataProperty,
                                    base::Value(uidata_blob));
  service_test_->SetServiceProperty(
      kWifi2ServicePath, shill::kAutoConnectProperty, base::Value(false));

  // Update the profile entry.
  profile_test_->AddService(kUser1ProfilePath, kWifi2ServicePath);

  content::RunAllPendingInMessageLoop();

  const std::string user_policy_blob =
      "{ \"NetworkConfigurations\": ["
      "    { \"GUID\": \"stub_wifi2\","
      "      \"Type\": \"WiFi\","
      "      \"Name\": \"My WiFi Network\","
      "      \"WiFi\": {"
      "        \"HexSSID\": \"77696669325F50534B\","  // "wifi2_PSK"
      "        \"Passphrase\": \"passphrase\","
      "        \"Recommended\": [ \"AutoConnect\", \"Passphrase\" ],"
      "        \"Security\": \"WPA-PSK\" }"
      "    }"
      "  ],"
      "  \"Certificates\": [],"
      "  \"Type\": \"UnencryptedConfiguration\""
      "}";

  policy::PolicyMap policy;
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::WrapUnique(new base::Value(user_policy_blob)), nullptr);
  provider_.UpdateChromePolicy(policy);

  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(RunNetworkingSubtest("getManagedProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetErrorState) {
  chromeos::NetworkHandler::Get()->network_state_handler()->SetLastErrorForTest(
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
                       OnCertificateListsChangedEvent) {
  TestListener listener("eventListenerReady", base::Bind([]() {
                          chromeos::NetworkHandler::Get()
                              ->network_certificate_handler()
                              ->NotifyCertificatsChangedForTest();
                        }));
  EXPECT_TRUE(RunNetworkingSubtest("onCertificateListsChangedEvent"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, VerifyDestination) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyDestination")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       VerifyAndEncryptCredentials) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptCredentials")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, VerifyAndEncryptData) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptData")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       SetWifiTDLSEnabledState) {
  device_test_->SetTDLSState(shill::kTDLSConnectedState);
  EXPECT_TRUE(RunNetworkingSubtest("setWifiTDLSEnabledState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetWifiTDLSStatus) {
  device_test_->SetTDLSState(shill::kTDLSConnectedState);
  EXPECT_TRUE(RunNetworkingSubtest("getWifiTDLSStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       GetCaptivePortalStatus) {
  SetupCellular();

  NetworkPortalDetector::CaptivePortalState state;
  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
  detector()->SetDetectionResultsForTesting("stub_ethernet_guid", state);

  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE;
  detector()->SetDetectionResultsForTesting("stub_wifi1_guid", state);

  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  detector()->SetDetectionResultsForTesting("stub_wifi2_guid", state);

  state.status =
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED;
  detector()->SetDetectionResultsForTesting("stub_cellular1_guid", state);

  EXPECT_TRUE(RunNetworkingSubtest("getCaptivePortalStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       CaptivePortalNotification) {
  detector()->SetDefaultNetworkForTesting("wifi_guid");
  NetworkPortalDetector::CaptivePortalState state;
  state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
  detector()->SetDetectionResultsForTesting("wifi_guid", state);

  TestListener listener(
      "notifyPortalDetectorObservers",
      base::Bind(&NetworkPortalDetectorTestImpl::NotifyObserversForTesting,
                 base::Unretained(detector())));
  EXPECT_TRUE(RunNetworkingSubtest("captivePortalNotification")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, UnlockCellularSim) {
  SetupCellular();
  // Lock the SIM
  device_test_->SetSimLocked(kCellularDevicePath, true);
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
  device_test_->SetSimLocked(kCellularDevicePath, true);
  EXPECT_TRUE(RunNetworkingSubtest("cellularSimPuk")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetGlobalPolicy) {
  base::DictionaryValue global_config;
  global_config.SetKey(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      base::Value(true));
  global_config.SetKey(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect,
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
  ASSERT_TRUE(SetupCertificates());
  EXPECT_TRUE(RunNetworkingSubtest("getCertificateLists")) << message_;
}

// Tests subset of networking API for the networking API alias - to verify that
// using API methods and event does not cause access exceptions (due to
// missing permissions).
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, Alias) {
  SetupCellular();
  EXPECT_TRUE(RunPlatformAppTest("networking_private/alias")) << message_;
}

}  // namespace
