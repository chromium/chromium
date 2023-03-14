// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_net_utils.h"

#include <string>

#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "net/cert/scoped_nss_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace arc {
namespace {

constexpr char kNetworkStatePath[] = "test_path";

constexpr char kTestCellularDevicePath[] = "cellular_path";
constexpr char kTestCellularDeviceName[] = "cellular_name";
constexpr char kTestCellularDeviceInterface[] = "cellular_interface";
constexpr char kTestCellularDeviceGuestInterface[] = "guest_interface";

constexpr char kGuid[] = "guid";
constexpr char kBssid[] = "bssid";
constexpr char kHexSsid[] = "123456";
constexpr char kAddress[] = "8.8.8.8";
constexpr char kGateway[] = "8.8.8.4";
constexpr char kNameServer1[] = "1.1.1.1";
constexpr char kNameServer2[] = "2.2.2.2";
constexpr char kNameServerIpv6[] = "2001:4860:4860::8888";
constexpr int kPrefixLen = 16;
constexpr int kHostMtu = 32;
constexpr int kFrequency = 100;
constexpr int kSignalStrength = 80;
constexpr int kRssi = 50;

class ArcNetUtilsTest : public testing::Test {
 public:
  ArcNetUtilsTest() = default;

  ArcNetUtilsTest(const ArcNetUtilsTest&) = delete;

  void SetUp() override {
    network_state_ = std::make_unique<ash::NetworkState>(kNetworkStatePath);
    AddWifiDevice();
    SetUpNetworkState();
    ash::NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    ash::NetworkHandler::Shutdown();
    network_state_.reset();
  }

  ash::NetworkState* GetNetworkState() { return network_state_.get(); }

  std::unique_ptr<base::Value> dict;

  struct in_addr StringToIPv4Address(const std::string& buf) {
    struct in_addr addr = {};
    if (!inet_pton(AF_INET, buf.c_str(), &addr)) {
      memset(&addr, 0, sizeof(addr));
    }
    return addr;
  }

  struct in6_addr StringToIPv6Address(const std::string& buf) {
    struct in6_addr addr = {};
    if (!inet_pton(AF_INET6, buf.c_str(), &addr)) {
      memset(&addr, 0, sizeof(addr));
    }
    return addr;
  }

  base::Value::Dict GetShillDict() {
    base::Value::Dict static_ip_config;
    base::Value::List name_servers;
    base::Value::Dict shill_dict;

    name_servers.Append(kNameServer1);
    name_servers.Append(kNameServer2);
    name_servers.Append("0.0.0.0");

    static_ip_config.Set(shill::kAddressProperty, kAddress);
    static_ip_config.Set(shill::kGatewayProperty, kGateway);
    static_ip_config.Set(shill::kPrefixlenProperty, kPrefixLen);
    static_ip_config.Set(shill::kNameServersProperty, std::move(name_servers));
    static_ip_config.Set(shill::kMtuProperty, kHostMtu);

    shill_dict.Set(shill::kMeteredProperty, true);
    shill_dict.Set(shill::kStaticIPConfigProperty, std::move(static_ip_config));
    return shill_dict;
  }

 private:
  void AddWifiDevice() {
    helper_.device_test()->AddDevice(kTestCellularDevicePath, shill::kTypeWifi,
                                     kTestCellularDeviceName);
    helper_.device_test()->SetDeviceProperty(
        kTestCellularDevicePath, shill::kInterfaceProperty,
        base::Value(kTestCellularDeviceInterface),
        /*notify_changed=*/false);
  }

  void SetUpNetworkState() {
    network_state_->PropertyChanged(shill::kDeviceProperty,
                                    base::Value(kTestCellularDevicePath));
    network_state_->PropertyChanged(shill::kGuidProperty, base::Value(kGuid));
    network_state_->PropertyChanged(shill::kTypeProperty,
                                    base::Value(shill::kTypeWifi));
    network_state_->PropertyChanged(shill::kWifiBSsid, base::Value(kBssid));
    network_state_->PropertyChanged(shill::kWifiHexSsid, base::Value(kHexSsid));
    network_state_->PropertyChanged(shill::kSecurityClassProperty,
                                    base::Value(shill::kSecurityClassPsk));
    network_state_->PropertyChanged(shill::kWifiFrequency,
                                    base::Value(kFrequency));
    network_state_->PropertyChanged(shill::kSignalStrengthProperty,
                                    base::Value(kSignalStrength));
    network_state_->PropertyChanged(shill::kWifiSignalStrengthRssiProperty,
                                    base::Value(kRssi));
  }

  std::unique_ptr<ash::NetworkState> network_state_;
  base::test::TaskEnvironment task_environment_;
  ash::NetworkStateTestHelper helper_{
      /*use_default_devices_and_services=*/false};
};

TEST_F(ArcNetUtilsTest, TranslateEapMethod) {
  EXPECT_EQ(shill::kEapMethodLEAP,
            net_utils::TranslateEapMethod(arc::mojom::EapMethod::kLeap));
  EXPECT_EQ(shill::kEapMethodPEAP,
            net_utils::TranslateEapMethod(arc::mojom::EapMethod::kPeap));
  EXPECT_EQ(shill::kEapMethodTLS,
            net_utils::TranslateEapMethod(arc::mojom::EapMethod::kTls));
  EXPECT_EQ(shill::kEapMethodTTLS,
            net_utils::TranslateEapMethod(arc::mojom::EapMethod::kTtls));
  EXPECT_TRUE(
      net_utils::TranslateEapMethod(arc::mojom::EapMethod::kNone).empty());
}

TEST_F(ArcNetUtilsTest, TranslateInvalidEapMethod) {
  arc::mojom::EapMethod invalidEapMethod =
      static_cast<arc::mojom::EapMethod>(-1);
  EXPECT_TRUE(net_utils::TranslateEapMethod(invalidEapMethod).empty());
}

TEST_F(ArcNetUtilsTest, TranslateEapPhase2Method) {
  EXPECT_EQ(
      shill::kEapPhase2AuthTTLSPAP,
      net_utils::TranslateEapPhase2Method(arc::mojom::EapPhase2Method::kPap));
  EXPECT_EQ(shill::kEapPhase2AuthTTLSMSCHAP,
            net_utils::TranslateEapPhase2Method(
                arc::mojom::EapPhase2Method::kMschap));
  EXPECT_EQ(shill::kEapPhase2AuthTTLSMSCHAPV2,
            net_utils::TranslateEapPhase2Method(
                arc::mojom::EapPhase2Method::kMschapv2));
  EXPECT_TRUE(
      net_utils::TranslateEapPhase2Method(arc::mojom::EapPhase2Method::kNone)
          .empty());
}

TEST_F(ArcNetUtilsTest, TranslateInvalidEapPhase2Method) {
  arc::mojom::EapPhase2Method invalidEapPhase2Method =
      static_cast<arc::mojom::EapPhase2Method>(-1);
  EXPECT_TRUE(
      net_utils::TranslateEapPhase2Method(invalidEapPhase2Method).empty());
}

TEST_F(ArcNetUtilsTest, TranslateKeyManagement) {
  EXPECT_EQ(
      shill::kKeyManagementIEEE8021X,
      net_utils::TranslateKeyManagement(arc::mojom::KeyManagement::kIeee8021X));
  EXPECT_TRUE(
      net_utils::TranslateKeyManagement(arc::mojom::KeyManagement::kFtEap)
          .empty());
  EXPECT_TRUE(
      net_utils::TranslateKeyManagement(arc::mojom::KeyManagement::kFtPsk)
          .empty());
  EXPECT_TRUE(
      net_utils::TranslateKeyManagement(arc::mojom::KeyManagement::kFtSae)
          .empty());
  EXPECT_TRUE(
      net_utils::TranslateKeyManagement(arc::mojom::KeyManagement::kWpaEap)
          .empty());
  EXPECT_TRUE(net_utils::TranslateKeyManagement(
                  arc::mojom::KeyManagement::kWpaEapSha256)
                  .empty());
  EXPECT_TRUE(
      net_utils::TranslateKeyManagement(arc::mojom::KeyManagement::kWpaPsk)
          .empty());
  EXPECT_TRUE(net_utils::TranslateKeyManagement(arc::mojom::KeyManagement::kSae)
                  .empty());
  EXPECT_TRUE(
      net_utils::TranslateKeyManagement(arc::mojom::KeyManagement::kNone)
          .empty());
}

TEST_F(ArcNetUtilsTest, TranslateInvalidKeyManagement) {
  arc::mojom::KeyManagement invalidKeyManagement =
      static_cast<arc::mojom::KeyManagement>(-1);
  EXPECT_TRUE(net_utils::TranslateKeyManagement(invalidKeyManagement).empty());
}

TEST_F(ArcNetUtilsTest, TranslateWiFiSecurity) {
  EXPECT_EQ(arc::mojom::SecurityType::NONE,
            net_utils::TranslateWiFiSecurity(shill::kSecurityClassNone));
  EXPECT_EQ(arc::mojom::SecurityType::WEP_PSK,
            net_utils::TranslateWiFiSecurity(shill::kSecurityClassWep));
  EXPECT_EQ(arc::mojom::SecurityType::WPA_PSK,
            net_utils::TranslateWiFiSecurity(shill::kSecurityClassPsk));
  EXPECT_EQ(arc::mojom::SecurityType::WPA_EAP,
            net_utils::TranslateWiFiSecurity(shill::kSecurityClass8021x));
}

TEST_F(ArcNetUtilsTest, TranslateInvalidWiFiSecurity) {
  std::string invalidKeyManagement = "invalidSecurityType";
  EXPECT_EQ(arc::mojom::SecurityType::NONE,
            net_utils::TranslateWiFiSecurity(invalidKeyManagement));
}

TEST_F(ArcNetUtilsTest, TranslateConnectionState) {
  EXPECT_EQ(arc::mojom::ConnectionStateType::CONNECTED,
            net_utils::TranslateConnectionState(shill::kStateReady));
  EXPECT_EQ(arc::mojom::ConnectionStateType::CONNECTING,
            net_utils::TranslateConnectionState(shill::kStateAssociation));
  EXPECT_EQ(arc::mojom::ConnectionStateType::CONNECTING,
            net_utils::TranslateConnectionState(shill::kStateConfiguration));
  EXPECT_EQ(arc::mojom::ConnectionStateType::NOT_CONNECTED,
            net_utils::TranslateConnectionState(shill::kStateIdle));
  EXPECT_EQ(arc::mojom::ConnectionStateType::NOT_CONNECTED,
            net_utils::TranslateConnectionState(shill::kStateFailure));
  EXPECT_EQ(arc::mojom::ConnectionStateType::NOT_CONNECTED,
            net_utils::TranslateConnectionState(shill::kStateDisconnect));
  EXPECT_EQ(arc::mojom::ConnectionStateType::NOT_CONNECTED,
            net_utils::TranslateConnectionState(""));
  EXPECT_EQ(arc::mojom::ConnectionStateType::ONLINE,
            net_utils::TranslateConnectionState(shill::kStateOnline));
}

TEST_F(ArcNetUtilsTest, TranslateNetworkType) {
  EXPECT_EQ(arc::mojom::NetworkType::WIFI,
            net_utils::TranslateNetworkType(shill::kTypeWifi));
  EXPECT_EQ(arc::mojom::NetworkType::VPN,
            net_utils::TranslateNetworkType(shill::kTypeVPN));
  EXPECT_EQ(arc::mojom::NetworkType::ETHERNET,
            net_utils::TranslateNetworkType(shill::kTypeEthernet));
  EXPECT_EQ(arc::mojom::NetworkType::CELLULAR,
            net_utils::TranslateNetworkType(shill::kTypeCellular));
}

TEST_F(ArcNetUtilsTest, TranslateNetworkProperties) {
  base::Value::Dict shill_dict = GetShillDict();
  const arc::mojom::NetworkConfigurationPtr mojo =
      net_utils::TranslateNetworkProperties(GetNetworkState(), &shill_dict);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(mojo.is_null());

  EXPECT_EQ(kGuid, mojo->guid);
  EXPECT_EQ(arc::mojom::NetworkType::WIFI, mojo->type);
  EXPECT_EQ(true, mojo->is_metered);
  EXPECT_EQ(kTestCellularDeviceInterface, mojo->network_interface);

  EXPECT_EQ(16u, mojo->host_ipv4_prefix_length);
  EXPECT_EQ(kAddress, mojo->host_ipv4_address);
  EXPECT_EQ(kGateway, mojo->host_ipv4_gateway);
  EXPECT_EQ(2u, mojo->host_dns_addresses.value().size());
  EXPECT_EQ(kNameServer1, mojo->host_dns_addresses.value()[0]);
  EXPECT_EQ(kNameServer2, mojo->host_dns_addresses.value()[1]);
  EXPECT_EQ(32u, mojo->host_mtu);

  EXPECT_FALSE(mojo->wifi.is_null());
  EXPECT_EQ(kBssid, mojo->wifi->bssid);
  EXPECT_EQ(kHexSsid, mojo->wifi->hex_ssid);
  EXPECT_EQ(arc::mojom::SecurityType::WPA_PSK, mojo->wifi->security);
  EXPECT_EQ(kFrequency, mojo->wifi->frequency);
  EXPECT_EQ(kSignalStrength, mojo->wifi->signal_strength);
}

TEST_F(ArcNetUtilsTest, TranslateNetworkStates) {
  std::vector<patchpanel::NetworkDevice> network_devices;
  patchpanel::NetworkDevice device;

  // Set up test network device.
  device.set_guest_type(patchpanel::NetworkDevice::ARC);
  device.set_phys_ifname(kTestCellularDeviceInterface);
  device.set_guest_ifname(kTestCellularDeviceGuestInterface);
  // Set binary form of IP address.
  device.set_ipv4_addr(StringToIPv4Address(kAddress).s_addr);
  device.set_host_ipv4_addr(StringToIPv4Address(kGateway).s_addr);
  device.mutable_ipv4_subnet()->set_prefix_len(kPrefixLen);
  auto ipv4_addr = StringToIPv4Address(kNameServer1);
  device.set_dns_proxy_ipv4_addr(reinterpret_cast<const char*>(&ipv4_addr),
                                 sizeof(ipv4_addr));
  auto ipv6_addr = StringToIPv6Address(kNameServerIpv6);
  device.set_dns_proxy_ipv6_addr(reinterpret_cast<const char*>(&ipv6_addr),
                                 sizeof(ipv6_addr));
  network_devices.push_back(device);

  std::map<std::string, base::Value::Dict> shill_network_properties;
  shill_network_properties[kNetworkStatePath] = GetShillDict();

  ash::NetworkStateHandler::NetworkStateList network_states;
  network_states.push_back(GetNetworkState());
  std::vector<arc::mojom::NetworkConfigurationPtr> res =
      net_utils::TranslateNetworkStates(/*arc_vpn_path=*/"", network_states,
                                        shill_network_properties,
                                        network_devices);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, network_states.size());
  EXPECT_EQ(kNetworkStatePath, res[0]->service_name);
  EXPECT_EQ(kTestCellularDeviceGuestInterface, res[0]->arc_network_interface);
  EXPECT_EQ(kAddress, res[0]->arc_ipv4_address);
  EXPECT_EQ(kGateway, res[0]->arc_ipv4_gateway);
  EXPECT_EQ(16u, res[0]->arc_ipv4_prefix_length);
  EXPECT_EQ(kNameServer1, res[0]->dns_proxy_addresses.value()[0]);
  EXPECT_EQ(kNameServerIpv6, res[0]->dns_proxy_addresses.value()[1]);
}

}  // namespace
}  // namespace arc
