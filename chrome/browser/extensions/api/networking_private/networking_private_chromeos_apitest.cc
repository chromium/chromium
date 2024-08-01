// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/networking_private/networking_private_chromeos.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_certificate_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

// This tests the Chrome OS implementation of the networkingPrivate API
// (NetworkingPrivateChromeOS). Note: The test expectations for chromeos, and
// win/mac (NetworkingPrivateServiceClient) are different to reflect the
// different implementations, but should be kept similar where possible.

using testing::_;
using testing::Return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
using ash::ShillDeviceClient;
using ash::ShillIPConfigClient;
using ash::ShillManagerClient;
using ash::ShillProfileClient;
using ash::ShillServiceClient;
using ash::UserDataAuthClient;

using extensions::NetworkingPrivateChromeOS;
using extensions::NetworkingPrivateDelegate;
using extensions::NetworkingPrivateDelegateFactory;
#endif

namespace {

const char kCellular1ServicePath[] = "stub_cellular1";
const char kCellularDevicePath[] = "/device/stub_cellular_device1";
const char kEthernetDevicePath[] = "/device/stub_ethernet_device";
const char kIPConfigPath[] = "/ipconfig/ipconfig1";
const char kUser1ProfilePath[] = "/profile/user1/shill";
const char kWifi1ServicePath[] = "stub_wifi1";
const char kWifi2ServicePath[] = "stub_wifi2";
const char kWifiDevicePath[] = "/device/stub_wifi_device1";

class NetworkingPrivateChromeOSApiTestBase
    : public extensions::ExtensionApiTest {
 public:
  // From extensions::ExtensionApiTest
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    // Allowlist the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        "epcifkihnkjgphfkloaaleeakhpmgdmn");
  }

  bool RunNetworkingSubtest(const std::string& test) {
    const std::string arg =
        base::StringPrintf("{\"test\": \"%s\"}", test.c_str());
    return RunExtensionTest(
        "networking_private/chromeos",
        {.custom_arg = arg.c_str(), .launch_as_platform_app = true});
  }

  void ConfigFakeNetwork() {
    ClearDevices();
    ClearServices();

    std::string userhash = GetSanitizedActiveUsername();

    // Sends a notification about the added profile.
    AddProfile(kUser1ProfilePath, userhash);

    // Add IPConfigs
    base::Value::Dict ipconfig;
    ipconfig.Set(shill::kAddressProperty, "0.0.0.0");
    ipconfig.Set(shill::kGatewayProperty, "0.0.0.1");
    ipconfig.Set(shill::kPrefixlenProperty, 0);
    ipconfig.Set(shill::kMethodProperty, shill::kTypeIPv4);
    AddIPConfig(kIPConfigPath, std::move(ipconfig));

    // Add Devices
    AddDevice(kEthernetDevicePath, shill::kTypeEthernet,
              "stub_ethernet_device1");

    AddDevice(kWifiDevicePath, shill::kTypeWifi, "stub_wifi_device1");
    base::Value::List wifi_ip_configs;
    wifi_ip_configs.Append(kIPConfigPath);
    SetDeviceProperty(kWifiDevicePath, shill::kIPConfigsProperty,
                      base::Value(std::move(wifi_ip_configs)));
    SetDeviceProperty(kWifiDevicePath, shill::kAddressProperty,
                      base::Value("001122aabbcc"));

    // Add Services
    AddService("stub_ethernet", "eth0", shill::kTypeEthernet,
               shill::kStateOnline);
    SetServiceProperty("stub_ethernet", shill::kProfileProperty,
                       base::Value(GetSharedProfilePath()));
    AddServiceToProfile(GetSharedProfilePath(), "stub_ethernet");

    AddService(kWifi1ServicePath, "wifi1", shill::kTypeWifi,
               shill::kStateOnline);
    SetServiceProperty(kWifi1ServicePath, shill::kSecurityClassProperty,
                       base::Value(shill::kSecurityClassWep));
    SetServiceProperty(kWifi1ServicePath, shill::kWifiBSsid,
                       base::Value("00:01:02:03:04:05"));
    SetServiceProperty(kWifi1ServicePath, shill::kSignalStrengthProperty,
                       base::Value(40));
    SetServiceProperty(kWifi1ServicePath, shill::kProfileProperty,
                       base::Value(kUser1ProfilePath));
    SetServiceProperty(kWifi1ServicePath, shill::kConnectableProperty,
                       base::Value(true));
    SetServiceProperty(kWifi1ServicePath, shill::kDeviceProperty,
                       base::Value(kWifiDevicePath));
    base::Value::Dict static_ipconfig;
    static_ipconfig.Set(shill::kAddressProperty, "1.2.3.4");
    static_ipconfig.Set(shill::kGatewayProperty, "0.0.0.0");
    static_ipconfig.Set(shill::kPrefixlenProperty, 1);
    SetServiceProperty(kWifi1ServicePath, shill::kStaticIPConfigProperty,
                       base::Value(std::move(static_ipconfig)));
    base::Value::List frequencies1;
    frequencies1.Append(2400);
    SetServiceProperty(kWifi1ServicePath, shill::kWifiFrequencyListProperty,
                       base::Value(std::move(frequencies1)));
    SetServiceProperty(kWifi1ServicePath, shill::kWifiFrequency,
                       base::Value(2400));
    AddServiceToProfile(kUser1ProfilePath, kWifi1ServicePath);

    AddService(kWifi2ServicePath, "wifi2_PSK", shill::kTypeWifi,
               shill::kStateIdle);
    SetServiceProperty(kWifi2ServicePath, shill::kSecurityClassProperty,
                       base::Value(shill::kSecurityClassPsk));
    SetServiceProperty(kWifi2ServicePath, shill::kSignalStrengthProperty,
                       base::Value(80));
    SetServiceProperty(kWifi2ServicePath, shill::kConnectableProperty,
                       base::Value(true));

    base::Value::List frequencies2;
    frequencies2.Append(2400);
    frequencies2.Append(5000);
    SetServiceProperty(kWifi2ServicePath, shill::kWifiFrequencyListProperty,
                       base::Value(std::move(frequencies2)));
    SetServiceProperty(kWifi2ServicePath, shill::kWifiFrequency,
                       base::Value(5000));
    SetServiceProperty(kWifi2ServicePath, shill::kProfileProperty,
                       base::Value(kUser1ProfilePath));
    AddServiceToProfile(kUser1ProfilePath, kWifi2ServicePath);

    AddService("stub_vpn1", "vpn1", shill::kTypeVPN, shill::kStateOnline);
    SetServiceProperty("stub_vpn1", shill::kProviderTypeProperty,
                       base::Value(shill::kProviderOpenVpn));
    AddServiceToProfile(kUser1ProfilePath, "stub_vpn1");

    AddService("stub_vpn2", "vpn2", shill::kTypeVPN, shill::kStateIdle);
    SetServiceProperty("stub_vpn2", shill::kProviderTypeProperty,
                       base::Value(shill::kProviderThirdPartyVpn));
    SetServiceProperty("stub_vpn2", shill::kProviderHostProperty,
                       base::Value("third_party_provider_extension_id"));
    AddServiceToProfile(kUser1ProfilePath, "stub_vpn2");
  }

  virtual void SetupCellular() {
    // Add a Cellular GSM Device.
    AddDevice(kCellularDevicePath, shill::kTypeCellular,
              "stub_cellular_device1");
    base::Value::Dict home_provider;
    home_provider.Set("name", "Cellular1_Provider");
    home_provider.Set("code", "000000");
    home_provider.Set("country", "us");
    SetDeviceProperty(kCellularDevicePath, shill::kHomeProviderProperty,
                      base::Value(std::move(home_provider)));
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
    SetSimLocked(kCellularDevicePath, false);

    // Add the Cellular Service.
    AddService(kCellular1ServicePath, "cellular1", shill::kTypeCellular,
               shill::kStateIdle);
    SetServiceProperty(kCellular1ServicePath,
                       shill::kCellularAllowRoamingProperty,
                       base::Value(false));
    SetServiceProperty(kCellular1ServicePath, shill::kAutoConnectProperty,
                       base::Value(true));
    SetServiceProperty(kCellular1ServicePath, shill::kIccidProperty,
                       base::Value("test_iccid"));
    SetServiceProperty(kCellular1ServicePath, shill::kNetworkTechnologyProperty,
                       base::Value(shill::kNetworkTechnologyGsm));
    SetServiceProperty(kCellular1ServicePath, shill::kActivationStateProperty,
                       base::Value(shill::kActivationStateNotActivated));
    SetServiceProperty(kCellular1ServicePath, shill::kRoamingStateProperty,
                       base::Value(shill::kRoamingStateHome));

    AddServiceToProfile(kUser1ProfilePath, kCellular1ServicePath);
  }

  virtual std::string GetSanitizedActiveUsername() = 0;

  virtual void AddDevice(const std::string& device_path,
                         const std::string& type,
                         const std::string& name) = 0;
  virtual void SetDeviceProperty(const std::string& device_path,
                                 const std::string& name,
                                 const base::Value& value) = 0;
  virtual void SetSimLocked(const std::string& device_path, bool enabled) = 0;
  virtual void ClearDevices() = 0;
  virtual void AddService(const std::string& service_path,
                          const std::string& name,
                          const std::string& type,
                          const std::string& state) = 0;
  virtual void ClearServices() = 0;
  virtual void SetServiceProperty(const std::string& service_path,
                                  const std::string& property,
                                  const base::Value& value) = 0;
  virtual void AddProfile(const std::string& profile_path,
                          const std::string& userhash) = 0;

  virtual void AddServiceToProfile(const std::string& profile_path,
                                   const std::string& service_path) = 0;
  virtual std::string GetSharedProfilePath() = 0;
  virtual void AddIPConfig(const std::string& ip_config_path,
                           base::Value::Dict properties) = 0;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

class NetworkingPrivateChromeOSApiTestAsh
    : public NetworkingPrivateChromeOSApiTestBase {
 public:
  NetworkingPrivateChromeOSApiTestAsh() = default;

  NetworkingPrivateChromeOSApiTestAsh(
      const NetworkingPrivateChromeOSApiTestAsh&) = delete;
  NetworkingPrivateChromeOSApiTestAsh& operator=(
      const NetworkingPrivateChromeOSApiTestAsh&) = delete;

  static std::unique_ptr<KeyedService> CreateNetworkingPrivateDelegate(
      content::BrowserContext* context) {
    std::unique_ptr<NetworkingPrivateDelegate> result(
        new NetworkingPrivateChromeOS(context));
    std::unique_ptr<NetworkingPrivateDelegate::UIDelegate> ui_delegate(
        new UIDelegateStub);
    result->set_ui_delegate(std::move(ui_delegate));
    return result;
  }

  void SetupTether() {
    ash::NetworkStateHandler* network_state_handler =
        ash::NetworkHandler::Get()->network_state_handler();
    network_state_handler->SetTetherTechnologyState(
        ash::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
    network_state_handler->AddTetherNetworkState(
        "tetherGuid1", "tetherName1", "tetherCarrier1",
        50 /* battery_percentage */, 75 /* signal_strength */,
        true /* has_connected_to_host */);
    network_state_handler->AddTetherNetworkState(
        "tetherGuid2", "tetherName2", "tetherCarrier2",
        75 /* battery_percentage */, 100 /* signal_strength */,
        false /* has_connected_to_host */);
  }

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

  // extensions::ExtensionApiTest overrides:

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();

    NetworkingPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&CreateNetworkingPrivateDelegate));

    network_handler_test_helper_ =
        std::make_unique<ash::NetworkHandlerTestHelper>();

    ConfigFakeNetwork();

    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    network_handler_test_helper_->RegisterPrefs(user_prefs_.registry(),
                                                local_state_.registry());

    network_handler_test_helper_->InitializePrefs(&user_prefs_, &local_state_);

    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override { network_handler_test_helper_.reset(); }

  // NetworkingPrivateChromeOSApiTestBase overrides:

  void SetUpCommandLine(base::CommandLine* command_line) override {
    NetworkingPrivateChromeOSApiTestBase::SetUpCommandLine(command_line);

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

  std::string GetSanitizedActiveUsername() override {
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    user_manager::User* user = user_manager->GetActiveUser();
    CHECK(user);
    std::string userhash;
    ::user_data_auth::GetSanitizedUsernameRequest request;
    request.set_username(
        cryptohome::CreateAccountIdentifierFromAccountId(user->GetAccountId())
            .account_id());
    ash::CryptohomeMiscClient::Get()->GetSanitizedUsername(
        request,
        base::BindOnce(
            [](std::string* out,
               std::optional<::user_data_auth::GetSanitizedUsernameReply>
                   result) {
              CHECK(result.has_value());
              *out = result->sanitized_username();
            },
            &userhash));
    base::RunLoop().RunUntilIdle();
    CHECK(!userhash.empty());
    return userhash;
  }

  void SetupCellular() override {
    UIDelegateStub::s_show_account_details_called_ = 0;
    NetworkingPrivateChromeOSApiTestBase::SetupCellular();
    base::RunLoop().RunUntilIdle();
  }

  void AddDevice(const std::string& device_path,
                 const std::string& type,
                 const std::string& name) override {
    device_test()->AddDevice(device_path, type, name);
  }

  void ClearDevices() override { device_test()->ClearDevices(); }

  void SetDeviceProperty(const std::string& device_path,
                         const std::string& name,
                         const base::Value& value) override {
    device_test()->SetDeviceProperty(device_path, name, value,
                                     /*notify_changed=*/true);
  }

  void SetSimLocked(const std::string& device_path, bool enabled) override {
    device_test()->SetSimLocked(device_path, enabled);
  }

  void AddService(const std::string& service_path,
                  const std::string& name,
                  const std::string& type,
                  const std::string& state) override {
    service_test()->AddService(service_path, service_path + "_guid", name, type,
                               state, true /* add_to_visible */);
  }

  void ClearServices() override { service_test()->ClearServices(); }

  void SetServiceProperty(const std::string& service_path,
                          const std::string& property,
                          const base::Value& value) override {
    service_test()->SetServiceProperty(service_path, property, value);
  }

  void AddIPConfig(const std::string& ip_config_path,
                   base::Value::Dict properties) override {
    network_handler_test_helper_->ip_config_test()->AddIPConfig(
        ip_config_path, std::move(properties));
  }

  void AddProfile(const std::string& profile_path,
                  const std::string& userhash) override {
    profile_test()->AddProfile(profile_path, userhash);
  }

  void AddServiceToProfile(const std::string& profile_path,
                           const std::string& service_path) override {
    profile_test()->AddService(profile_path, service_path);
  }

  std::string GetSharedProfilePath() override {
    return ShillProfileClient::GetSharedProfilePath();
  }

 protected:
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
};
#else
class NetworkingPrivateChromeOSApiTestLacros
    : public NetworkingPrivateChromeOSApiTestBase {
 public:
  NetworkingPrivateChromeOSApiTestLacros() {}

  NetworkingPrivateChromeOSApiTestLacros(
      const NetworkingPrivateChromeOSApiTestLacros&) = delete;
  NetworkingPrivateChromeOSApiTestLacros& operator=(
      const NetworkingPrivateChromeOSApiTestLacros&) = delete;

  bool SetUpAsh() {
    auto* service = chromeos::LacrosService::Get();
    if (!service->IsAvailable<crosapi::mojom::TestController>() ||
        service->GetInterfaceVersion<crosapi::mojom::TestController>() <
            static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                                 kBindShillClientTestInterfaceMinVersion)) {
      LOG(ERROR) << "Unsupported ash version.";
      return false;
    }

    base::test::TestFuture<void> future;
    service->GetRemote<crosapi::mojom::TestController>()
        ->BindShillClientTestInterface(shill_test_.BindNewPipeAndPassReceiver(),
                                       future.GetCallback());
    EXPECT_TRUE(future.Wait());

    ConfigFakeNetwork();

    return true;
  }

  // NetworkingPrivateChromeOSApiTestBase overrides

  std::string GetSanitizedActiveUsername() override {
    auto* service = chromeos::LacrosService::Get();
    if (!service->IsAvailable<crosapi::mojom::TestController>() ||
        service->GetInterfaceVersion<crosapi::mojom::TestController>() <
            static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                                 kGetSanitizedActiveUsernameMinVersion)) {
      LOG(ERROR) << "Unsupported ash version.";
      return "";
    }

    base::test::TestFuture<const std::string&> future;
    service->GetRemote<crosapi::mojom::TestController>()
        ->GetSanitizedActiveUsername(future.GetCallback());
    return future.Take();
  }

  void AddDevice(const std::string& device_path,
                 const std::string& type,
                 const std::string& name) override {
    base::test::TestFuture<void> future;
    shill_test_->AddDevice(device_path, type, name, future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  void SetDeviceProperty(const std::string& device_path,
                         const std::string& name,
                         const base::Value& value) override {
    base::test::TestFuture<void> future;
    shill_test_->SetDeviceProperty(device_path, name, value.Clone(),
                                   /*notify_changed=*/true,
                                   future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  void SetSimLocked(const std::string& device_path, bool enabled) override {
    base::test::TestFuture<void> future;
    shill_test_->SetSimLocked(device_path, enabled, future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  void ClearDevices() override {
    base::test::TestFuture<void> future;
    shill_test_->ClearDevices(future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  void AddService(const std::string& service_path,
                  const std::string& name,
                  const std::string& type,
                  const std::string& state) override {
    base::test::TestFuture<void> future;
    shill_test_->AddService(service_path, service_path + "_guid", name, type,
                            state, true /* add_to_visible */,
                            future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  void ClearServices() override {
    base::test::TestFuture<void> future;
    shill_test_->ClearServices(future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  void SetServiceProperty(const std::string& service_path,
                          const std::string& property,
                          const base::Value& value) override {
    base::test::TestFuture<void> future;
    shill_test_->SetServiceProperty(service_path, property, value.Clone(),
                                    future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  void AddIPConfig(const std::string& ip_config_path,
                   base::Value::Dict properties) override {
    base::test::TestFuture<void> future;
    shill_test_->AddIPConfig(ip_config_path, base::Value(std::move(properties)),
                             future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  void AddProfile(const std::string& profile_path,
                  const std::string& userhash) override {
    base::test::TestFuture<void> future;
    shill_test_->AddProfile(profile_path, userhash, future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  void AddServiceToProfile(const std::string& profile_path,
                           const std::string& service_path) override {
    base::test::TestFuture<void> future;
    shill_test_->AddServiceToProfile(profile_path, service_path,
                                     future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  std::string GetSharedProfilePath() override {
    // TODO(crbug.com/): get this information from Ash
    const char kSharedProfilePath[] = "/profile/default";
    return kSharedProfilePath;
  }

 protected:
  mojo::Remote<crosapi::mojom::ShillClientTestInterface> shill_test_;
};
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
using NetworkingPrivateChromeOSApiTest = NetworkingPrivateChromeOSApiTestLacros;
#else
using NetworkingPrivateChromeOSApiTest = NetworkingPrivateChromeOSApiTestAsh;
#endif

// Place each subtest into a separate browser test so that the stub networking
// library state is reset for each subtest run. This way they won't affect each
// other.

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, StartConnect) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("startConnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, StartDisconnect) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnect")) << message_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, StartActivate) {
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("startActivate")) << message_;
  EXPECT_EQ(1, UIDelegateStub::s_show_account_details_called_);
}
#endif

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       StartConnectNonexistent) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("startConnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       StartDisconnectNonexistent) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       StartGetPropertiesNonexistent) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("startGetPropertiesNonexistent"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetNetworks) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  // Hide stub_wifi2.
  SetServiceProperty(kWifi2ServicePath, shill::kVisibleProperty,
                     base::Value(false));
  // Add a couple of additional networks that are not configured (saved).
  AddService("stub_wifi3", "wifi3", shill::kTypeWifi, shill::kStateIdle);
  AddService("stub_wifi4", "wifi4", shill::kTypeWifi, shill::kStateIdle);
  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(RunNetworkingSubtest("getNetworks")) << message_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetVisibleNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworks")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       GetVisibleNetworksWifi) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworksWifi")) << message_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       GetDeviceStatesLacros) {
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
  EXPECT_TRUE(RunNetworkingSubtest("getDeviceStatesLacros")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, RequestNetworkScan) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("requestNetworkScan")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       RequestNetworkScanCellular) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("requestNetworkScanCellular")) << message_;
}

// Properties are filtered and translated through
// ShillToONCTranslator::TranslateWiFiWithState
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetProperties) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("getProperties")) << message_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       GetCellularProperties) {
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("getPropertiesCellular")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetState) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("getState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetStateNonExistent) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("getStateNonExistent")) << message_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, ForgetNetwork) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("forgetNetwork")) << message_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  ash::NetworkHandler::Get()->network_state_handler()->SetErrorForTest(
      kWifi1ServicePath, "TestErrorState");
  EXPECT_TRUE(RunNetworkingSubtest("getErrorState")) << message_;
}
#endif

// TODO(crbug.com/41496066): This test is flaky.
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       DISABLED_OnNetworksChangedEventConnect) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventConnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnNetworksChangedEventDisconnect) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventDisconnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnNetworkListChangedEvent) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("onNetworkListChangedEvent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnDeviceStateListChangedEvent) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  EXPECT_TRUE(RunNetworkingSubtest("onDeviceStateListChangedEvent"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnDeviceScanningChangedEvent) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("onDeviceScanningChangedEvent")) << message_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       OnCertificateListsChangedEvent) {
  ExtensionTestMessageListener listener("eventListenerReady");
  listener.SetOnSatisfied(base::BindOnce([](const std::string& message) {
    ash::NetworkHandler::Get()
        ->network_certificate_handler()
        ->AddAuthorityCertificateForTest("authority_cert");
  }));
  EXPECT_TRUE(RunNetworkingSubtest("onCertificateListsChangedEvent"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       GetCaptivePortalStatus) {
  // Ethernet defaults to online.
  // Set wifi1 to idle with captive portal status mapping 'Offline'.
  // Set wifi2 to redirect-found with captive portal statu smapping 'Portal'.
  SetServiceProperty(kWifi1ServicePath, shill::kStateProperty,
                     base::Value(shill::kStateIdle));
  SetServiceProperty(kWifi2ServicePath, shill::kStateProperty,
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

  ExtensionTestMessageListener listener("notifyPortalDetectorObservers");
  listener.SetOnSatisfied(
      base::BindLambdaForTesting([&](const std::string& message) {
        SetServiceProperty(kWifi1ServicePath, shill::kStateProperty,
                           base::Value(shill::kStateRedirectFound));
      }));

  EXPECT_TRUE(RunNetworkingSubtest("captivePortalNotification")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, UnlockCellularSim) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  SetupCellular();
  // Lock the SIM
  SetSimLocked(kCellularDevicePath, true);
  EXPECT_TRUE(RunNetworkingSubtest("unlockCellularSim")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, SetCellularSimState) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  SetupCellular();
  EXPECT_TRUE(RunNetworkingSubtest("setCellularSimState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest,
                       SelectCellularMobileNetwork) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  SetupCellular();
  // Create fake list of found networks.
  base::Value::List found_networks =
      base::Value::List()
          .Append(base::Value::Dict()
                      .Set(shill::kNetworkIdProperty, "network1")
                      .Set(shill::kTechnologyProperty, "GSM")
                      .Set(shill::kStatusProperty, "current"))
          .Append(base::Value::Dict()
                      .Set(shill::kNetworkIdProperty, "network2")
                      .Set(shill::kTechnologyProperty, "GSM")
                      .Set(shill::kStatusProperty, "available"));
  SetDeviceProperty(kCellularDevicePath, shill::kFoundNetworksProperty,
                    base::Value(std::move(found_networks)));
  EXPECT_TRUE(RunNetworkingSubtest("selectCellularMobileNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, CellularSimPuk) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!SetUpAsh()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
#endif
  SetupCellular();
  // Lock the SIM
  SetSimLocked(kCellularDevicePath, true);
  EXPECT_TRUE(RunNetworkingSubtest("cellularSimPuk")) << message_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(NetworkingPrivateChromeOSApiTest, GetGlobalPolicy) {
  base::Value::Dict global_config;
  global_config.Set(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      true);
  global_config.Set(::onc::global_network_config::kAllowOnlyPolicyWiFiToConnect,
                    false);
  global_config.Set("SomeNewGlobalPolicy", false);
  ash::NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                  /*userhash=*/std::string(),
                  /*network_configs_onc=*/base::Value::List(), global_config);
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
  ash::NetworkHandler::Get()
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
#endif

}  // namespace
