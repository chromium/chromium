// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/networking_attributes_ash.h"

#include "base/logging.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/crosapi/mojom/networking_attributes.mojom.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/object_path.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace {

const char kErrorUserNotAffiliated[] =
    "Network attributes can only be read by an affiliated user.";
const char kErrorNetworkNotConnected[] =
    "Device is not connected to a network.";

constexpr char kFormattedMacAddress[] = "01:23:45:67:89:AB";
constexpr char kIpv4Address[] = "192.168.0.42";
constexpr char kIpv6Address[] = "fe80::1262:d0ff:fef5:e8a9";

constexpr char kWifiDevicePath[] = "/device/stub_wifi";
constexpr char kWifiServicePath[] = "/service/stub_wifi";
constexpr char kWifiIPConfigV4Path[] = "/ipconfig/stub_wifi-ipv4";
constexpr char kWifiIPConfigV6Path[] = "/ipconfig/stub_wifi-ipv6";

}  // namespace

namespace crosapi {

namespace {
void EvaluateGetNetworkDetailsResult(base::OnceClosure closure,
                                     mojom::GetNetworkDetailsResultPtr expected,
                                     mojom::GetNetworkDetailsResultPtr found) {
  ASSERT_EQ(expected->which(), found->which());
  if (expected->which() == mojom::GetNetworkDetailsResult::Tag::kErrorMessage) {
    ASSERT_EQ(expected->get_error_message(), found->get_error_message());
  } else {
    ASSERT_EQ(expected->get_network_details()->mac_address,
              found->get_network_details()->mac_address);
    ASSERT_EQ(expected->get_network_details()->ipv4_address,
              found->get_network_details()->ipv4_address);
    ASSERT_EQ(expected->get_network_details()->ipv6_address,
              found->get_network_details()->ipv6_address);
  }
  std::move(closure).Run();
}

void ShillErrorCallbackFunction(const std::string& error_name,
                                const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}
}  // namespace

class NetworkingAttributesAshTest : public testing::Test {
 public:
  class MockPropertyChangeObserver : public ash::ShillPropertyChangedObserver {
   public:
    MockPropertyChangeObserver() = default;
    ~MockPropertyChangeObserver() override = default;
    MOCK_METHOD2(OnPropertyChanged,
                 void(const std::string& name, const base::Value& value));
  };

  NetworkingAttributesAshTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~NetworkingAttributesAshTest() override = default;

  void SetUp() override {
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
    networking_attributes_ash_ = std::make_unique<NetworkingAttributesAsh>();
    networking_attributes_ash_->BindReceiver(
        networking_attributes_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override { networking_attributes_ash_.reset(); }

  void AddUser(bool is_affiliated = true) {
    AccountId account_id = AccountId::FromUserEmail("user@test.com");
    ash::FakeChromeUserManager* user_manager =
        static_cast<ash::FakeChromeUserManager*>(
            user_manager::UserManager::Get());
    const user_manager::User* user =
        user_manager->AddUserWithAffiliation(account_id, is_affiliated);
    user_manager->UserLoggedIn(account_id, user->username_hash(),
                               /*browser_restart=*/false, /*is_child=*/false);
    user_manager->SimulateUserProfileLoad(account_id);
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                 &profile_);
  }

  void SetUpShillState() {
    network_handler_test_helper_.device_test()->ClearDevices();
    network_handler_test_helper_.service_test()->ClearServices();

    base::DictionaryValue ipconfig_v4_dictionary;
    ipconfig_v4_dictionary.SetKey(shill::kAddressProperty,
                                  base::Value(kIpv4Address));
    ipconfig_v4_dictionary.SetKey(shill::kMethodProperty,
                                  base::Value(shill::kTypeIPv4));

    base::DictionaryValue ipconfig_v6_dictionary;
    ipconfig_v6_dictionary.SetKey(shill::kAddressProperty,
                                  base::Value(kIpv6Address));
    ipconfig_v6_dictionary.SetKey(shill::kMethodProperty,
                                  base::Value(shill::kTypeIPv6));

    network_handler_test_helper_.ip_config_test()->AddIPConfig(
        kWifiIPConfigV4Path, ipconfig_v4_dictionary);
    network_handler_test_helper_.ip_config_test()->AddIPConfig(
        kWifiIPConfigV6Path, ipconfig_v6_dictionary);

    base::ListValue ip_configs;
    ip_configs.Append(kWifiIPConfigV4Path);
    ip_configs.Append(kWifiIPConfigV6Path);

    network_handler_test_helper_.device_test()->AddDevice(
        kWifiDevicePath, shill::kTypeWifi, "stub_wifi_device");
    network_handler_test_helper_.service_test()->AddService(
        kWifiServicePath, "wifi_guid", "wifi_network_name", shill::kTypeWifi,
        shill::kStateOnline, /* visible= */ true);
    EXPECT_EQ(network_handler_test_helper_.GetServiceStringProperty(
                  kWifiServicePath, shill::kStateProperty),
              shill::kStateOnline);

    base::RunLoop device_client_mac_address_run_loop;
    ash::ShillDeviceClient* shill_device_client = ash::ShillDeviceClient::Get();
    shill_device_client->SetProperty(
        dbus::ObjectPath(kWifiDevicePath), shill::kAddressProperty,
        base::Value(kFormattedMacAddress),
        device_client_mac_address_run_loop.QuitClosure(),
        base::BindOnce(&ShillErrorCallbackFunction));
    device_client_mac_address_run_loop.Run();

    base::RunLoop device_client_ip_config_run_loop;
    shill_device_client->SetProperty(
        dbus::ObjectPath(kWifiDevicePath), shill::kIPConfigsProperty,
        ip_configs, device_client_ip_config_run_loop.QuitClosure(),
        base::BindOnce(&ShillErrorCallbackFunction));
    device_client_ip_config_run_loop.Run();

    testing::StrictMock<MockPropertyChangeObserver> observer;
    ash::ShillServiceClient* shill_service_client =
        ash::ShillServiceClient::Get();
    shill_service_client->AddPropertyChangedObserver(
        dbus::ObjectPath(kWifiServicePath), &observer);

    base::RunLoop service_client_run_loop;
    base::Value kConnectable(true);
    EXPECT_CALL(observer,
                OnPropertyChanged(shill::kConnectableProperty,
                                  testing::Eq(testing::ByRef(kConnectable))))
        .Times(1);
    shill_service_client->SetProperty(
        dbus::ObjectPath(kWifiServicePath), shill::kConnectableProperty,
        kConnectable, service_client_run_loop.QuitClosure(),
        base::BindOnce(&ShillErrorCallbackFunction));
    service_client_run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&observer);

    const ash::DeviceState* device_state =
        ash::NetworkHandler::Get()->network_state_handler()->GetDeviceState(
            kWifiDevicePath);
    EXPECT_EQ(device_state->mac_address(), kFormattedMacAddress);
    EXPECT_EQ(device_state->GetIpAddressByType(shill::kTypeIPv4), kIpv4Address);
    EXPECT_EQ(device_state->GetIpAddressByType(shill::kTypeIPv6), kIpv6Address);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  mojo::Remote<mojom::NetworkingAttributes> networking_attributes_remote_;
  std::unique_ptr<NetworkingAttributesAsh> networking_attributes_ash_;
  ash::NetworkHandlerTestHelper network_handler_test_helper_;

  ScopedTestingLocalState local_state_;
};

TEST_F(NetworkingAttributesAshTest, GetNetworkDetailsUserNotAffiliated) {
  AddUser(/*is_affiliated=*/false);

  mojom::GetNetworkDetailsResultPtr expected_result_ptr =
      mojom::GetNetworkDetailsResult::NewErrorMessage(kErrorUserNotAffiliated);

  base::RunLoop run_loop;
  networking_attributes_remote_->GetNetworkDetails(
      base::BindOnce(&EvaluateGetNetworkDetailsResult, run_loop.QuitClosure(),
                     std::move(expected_result_ptr)));
  run_loop.Run();
}

TEST_F(NetworkingAttributesAshTest, GetNetworkDetailsNetworkNotConnected) {
  AddUser();

  mojom::GetNetworkDetailsResultPtr expected_result_ptr =
      mojom::GetNetworkDetailsResult::NewErrorMessage(
          kErrorNetworkNotConnected);

  base::RunLoop run_loop;
  networking_attributes_remote_->GetNetworkDetails(
      base::BindOnce(&EvaluateGetNetworkDetailsResult, run_loop.QuitClosure(),
                     std::move(expected_result_ptr)));
  run_loop.Run();
}

TEST_F(NetworkingAttributesAshTest, GetNetworkDetailsSuccess) {
  AddUser();
  SetUpShillState();

  net::IPAddress ipv4_expected;
  net::IPAddress ipv6_expected;
  ASSERT_TRUE(ipv4_expected.AssignFromIPLiteral(kIpv4Address));
  ASSERT_TRUE(ipv6_expected.AssignFromIPLiteral(kIpv6Address));

  mojom::NetworkDetailsPtr expected_network_details =
      mojom::NetworkDetails::New();
  expected_network_details->mac_address = kFormattedMacAddress;
  expected_network_details->ipv4_address = ipv4_expected;
  expected_network_details->ipv6_address = ipv6_expected;
  mojom::GetNetworkDetailsResultPtr expected_result_ptr =
      mojom::GetNetworkDetailsResult::NewNetworkDetails(
          std::move(expected_network_details));

  base::RunLoop run_loop;
  networking_attributes_remote_->GetNetworkDetails(
      base::BindOnce(&EvaluateGetNetworkDetailsResult, run_loop.QuitClosure(),
                     std::move(expected_result_ptr)));
  run_loop.Run();
}

}  // namespace crosapi
