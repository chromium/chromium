// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_net_utils.h"

#include <memory>
#include <string>

#include "ash/components/arc/mojom/arc_wifi.mojom.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "net/cert/scoped_nss_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace arc {
namespace {

constexpr char kNetworkStatePath[] = "test_path";

constexpr char kTestEthDeviceInterface[] = "eth0";
constexpr char kTestEthDeviceGuestInterface[] = "eth0";
constexpr char kTestWiFiDevicePath[] = "wifi_path";
constexpr char kTestWiFiDeviceName[] = "wifi_name";
constexpr char kTestWiFiDeviceInterface[] = "wlan0";
constexpr char kTestWiFiDeviceGuestInterface[] = "wlan0";

constexpr char kGuid[] = "guid";
constexpr char kBssid[] = "bssid";
constexpr char kHexSsid[] = "123456";
constexpr char kTestWiFiAddress[] = "192.168.2.1";
constexpr char kDestinationAddress[] = "192.168.1.1";
constexpr char kIPv4Address[] = "192.168.0.2";
constexpr int kIPv4PrefixLen = 16;
constexpr char kIPv4Gateway[] = "192.168.0.1";
constexpr char kIPv6Address[] = "fd00::1";
constexpr int kIPv6PrefixLen = 64;
constexpr char kIPv6Gateway[] = "fd00::2";
constexpr char kNameServer1[] = "1.1.1.1";
constexpr char kNameServer2[] = "2.2.2.2";
constexpr char kNameServerIPv6[] = "2001:4860:4860::8888";
constexpr char kDomainName[] = "domain";
constexpr char kIncludedRoute[] = "1.2.3.4/24";
constexpr char kExcludedRoute[] = "4.3.2.1/24";
constexpr char kFqdn[] = "www.example.com";
constexpr int kHostMtu = 32;
constexpr int kFrequency = 100;
constexpr int kSignalStrength = 80;
constexpr int kRssi = 50;
constexpr int kPort1 = 20000;
constexpr int kPort2 = 30000;
// kTestWiFiAddress and kDestinationAddress in network byte order.
constexpr uint32_t kTestWiFiAddressNBO = 0x102A8C0u;
constexpr uint32_t kDestinationAddressNBO = 0x101A8C0u;

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

  static struct in_addr StringToIPv4Address(const std::string& buf) {
    struct in_addr addr = {};
    if (!inet_pton(AF_INET, buf.c_str(), &addr)) {
      memset(&addr, 0, sizeof(addr));
    }
    return addr;
  }

  static struct in6_addr StringToIPv6Address(const std::string& buf) {
    struct in6_addr addr = {};
    if (!inet_pton(AF_INET6, buf.c_str(), &addr)) {
      memset(&addr, 0, sizeof(addr));
    }
    return addr;
  }

  static patchpanel::NetworkDevice GetDevice(
      const std::string& phys_ifname,
      const std::string& guest_ifname,
      std::optional<patchpanel::NetworkDevice::TechnologyType>
          technology_type) {
    patchpanel::NetworkDevice device;
    // Set up test network device.
    device.set_guest_type(patchpanel::NetworkDevice::ARC);
    device.set_phys_ifname(phys_ifname);
    device.set_guest_ifname(guest_ifname);
    if (technology_type.has_value()) {
      device.set_technology_type(technology_type.value());
    }
    return device;
  }

  base::Value::Dict GetShillDict() {
    base::Value::Dict shill_dict;
    shill_dict.Set(shill::kMeteredProperty, true);
    return shill_dict;
  }

  mojom::SocketConnectionEventPtr GetMojomSocketConnectionEvent() {
    mojom::SocketConnectionEventPtr mojom =
        arc::mojom::SocketConnectionEvent::New();
    mojom->src_addr = net::IPAddress::FromIPLiteral(kTestWiFiAddress).value();
    mojom->dst_addr =
        net::IPAddress::FromIPLiteral(kDestinationAddress).value();
    mojom->src_port = kPort1;
    mojom->dst_port = kPort2;
    mojom->proto = arc::mojom::IpProtocol::kTcp;
    mojom->event = arc::mojom::SocketEvent::kOpen;
    mojom->qos_category = arc::mojom::QosCategory::kRealtimeInteractive;
    return mojom;
  }

 private:
  void AddWifiDevice() {
    helper_.device_test()->AddDevice(kTestWiFiDevicePath, shill::kTypeWifi,
                                     kTestWiFiDeviceName);
    helper_.device_test()->SetDeviceProperty(
        kTestWiFiDevicePath, shill::kInterfaceProperty,
        base::Value(kTestWiFiDeviceInterface),
        /*notify_changed=*/false);
  }

  void SetUpNetworkState() {
    network_state_->PropertyChanged(shill::kVisibleProperty, base::Value(true));
    network_state_->PropertyChanged(shill::kStateProperty,
                                    base::Value(shill::kStateOnline));
    network_state_->PropertyChanged(shill::kDeviceProperty,
                                    base::Value(kTestWiFiDevicePath));
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
    network_state_->PropertyChanged(shill::kMeteredProperty, base::Value(true));
    network_state_->PropertyChanged(shill::kPasspointFQDNProperty,
                                    base::Value(kFqdn));
    network_state_->PropertyChanged(shill::kWifiHiddenSsid, base::Value(true));

    // Set up NetworkConfig.
    base::Value::Dict properties;
    properties.Set(shill::kNetworkConfigIPv4AddressProperty,
                   base::StringPrintf("%s/%d", kIPv4Address, kIPv4PrefixLen));
    properties.Set(shill::kNetworkConfigIPv4GatewayProperty, kIPv4Gateway);
    properties.Set(shill::kNetworkConfigIPv6AddressesProperty,
                   base::Value::List().Append(base::StringPrintf(
                       "%s/%d", kIPv6Address, kIPv6PrefixLen)));
    properties.Set(shill::kNetworkConfigIPv6GatewayProperty, kIPv6Gateway);
    properties.Set(shill::kNetworkConfigNameServersProperty,
                   base::Value::List()
                       .Append(kNameServer1)
                       .Append(kNameServer2)
                       .Append(kNameServerIPv6));
    properties.Set(shill::kNetworkConfigSearchDomainsProperty,
                   base::Value::List().Append(kDomainName));
    properties.Set(shill::kNetworkConfigMTUProperty, kHostMtu);
    properties.Set(shill::kNetworkConfigIncludedRoutesProperty,
                   base::Value::List().Append(kIncludedRoute));
    properties.Set(shill::kNetworkConfigExcludedRoutesProperty,
                   base::Value::List().Append(kExcludedRoute));

    network_state_->PropertyChanged(shill::kNetworkConfigProperty,
                                    base::Value(std::move(properties)));
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

TEST_F(ArcNetUtilsTest, TranslateEapMethodToOnc) {
  EXPECT_EQ(onc::eap::kLEAP,
            net_utils::TranslateEapMethodToOnc(arc::mojom::EapMethod::kLeap));
  EXPECT_EQ(onc::eap::kPEAP,
            net_utils::TranslateEapMethodToOnc(arc::mojom::EapMethod::kPeap));
  EXPECT_EQ(onc::eap::kEAP_TLS,
            net_utils::TranslateEapMethodToOnc(arc::mojom::EapMethod::kTls));
  EXPECT_EQ(onc::eap::kEAP_TTLS,
            net_utils::TranslateEapMethodToOnc(arc::mojom::EapMethod::kTtls));
  EXPECT_TRUE(
      net_utils::TranslateEapMethodToOnc(arc::mojom::EapMethod::kNone).empty());
}

TEST_F(ArcNetUtilsTest, TranslateInvalidEapMethodToOnc) {
  arc::mojom::EapMethod invalidEapMethod =
      static_cast<arc::mojom::EapMethod>(-1);
  EXPECT_TRUE(net_utils::TranslateEapMethodToOnc(invalidEapMethod).empty());
}

TEST_F(ArcNetUtilsTest, TranslateEapPhase2MethodToOnc) {
  EXPECT_EQ(onc::eap::kPAP, net_utils::TranslateEapPhase2MethodToOnc(
                                arc::mojom::EapPhase2Method::kPap));
  EXPECT_EQ(onc::eap::kMSCHAP, net_utils::TranslateEapPhase2MethodToOnc(
                                   arc::mojom::EapPhase2Method::kMschap));
  EXPECT_EQ(onc::eap::kMSCHAPv2, net_utils::TranslateEapPhase2MethodToOnc(
                                     arc::mojom::EapPhase2Method::kMschapv2));
  EXPECT_TRUE(net_utils::TranslateEapPhase2MethodToOnc(
                  arc::mojom::EapPhase2Method::kNone)
                  .empty());
}

TEST_F(ArcNetUtilsTest, TranslateInvalidEapPhase2MethodToOnc) {
  arc::mojom::EapPhase2Method invalidEapPhase2Method =
      static_cast<arc::mojom::EapPhase2Method>(-1);
  EXPECT_TRUE(
      net_utils::TranslateEapPhase2MethodToOnc(invalidEapPhase2Method).empty());
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

TEST_F(ArcNetUtilsTest, TranslateKeyManagementToOnc) {
  EXPECT_EQ(onc::wifi::kWEP_8021X, net_utils::TranslateKeyManagementToOnc(
                                       arc::mojom::KeyManagement::kIeee8021X));
  EXPECT_TRUE(
      net_utils::TranslateKeyManagementToOnc(arc::mojom::KeyManagement::kFtEap)
          .empty());
  EXPECT_TRUE(
      net_utils::TranslateKeyManagementToOnc(arc::mojom::KeyManagement::kFtPsk)
          .empty());
  EXPECT_TRUE(
      net_utils::TranslateKeyManagementToOnc(arc::mojom::KeyManagement::kFtSae)
          .empty());
  EXPECT_TRUE(
      net_utils::TranslateKeyManagementToOnc(arc::mojom::KeyManagement::kWpaEap)
          .empty());
  EXPECT_TRUE(net_utils::TranslateKeyManagementToOnc(
                  arc::mojom::KeyManagement::kWpaEapSha256)
                  .empty());
  EXPECT_TRUE(
      net_utils::TranslateKeyManagementToOnc(arc::mojom::KeyManagement::kWpaPsk)
          .empty());
  EXPECT_TRUE(
      net_utils::TranslateKeyManagementToOnc(arc::mojom::KeyManagement::kSae)
          .empty());
  EXPECT_TRUE(
      net_utils::TranslateKeyManagementToOnc(arc::mojom::KeyManagement::kNone)
          .empty());
}

TEST_F(ArcNetUtilsTest, TranslateInvalidKeyManagementToOnc) {
  arc::mojom::KeyManagement invalidKeyManagement =
      static_cast<arc::mojom::KeyManagement>(-1);
  EXPECT_TRUE(
      net_utils::TranslateKeyManagementToOnc(invalidKeyManagement).empty());
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
            net_utils::TranslateConnectionState(shill::kStateDisconnecting));
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

TEST_F(ArcNetUtilsTest, FillConfigurationsFromState) {
  base::Value::Dict shill_dict = GetShillDict();
  auto mojo = arc::mojom::NetworkConfiguration::New();
  net_utils::FillConfigurationsFromState(GetNetworkState(), &shill_dict,
                                         mojo.get());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(mojo.is_null());

  EXPECT_EQ(kGuid, mojo->guid);
  EXPECT_EQ(arc::mojom::NetworkType::WIFI, mojo->type);
  EXPECT_EQ(true, mojo->is_metered);
  EXPECT_EQ(kTestWiFiDeviceInterface, mojo->network_interface);

  EXPECT_EQ(kIPv4Address, mojo->host_ipv4_address);
  EXPECT_EQ(uint32_t{kIPv4PrefixLen}, mojo->host_ipv4_prefix_length);
  EXPECT_EQ(kIPv4Gateway, mojo->host_ipv4_gateway);
  EXPECT_EQ(std::vector<std::string>({kIPv6Address}),
            mojo->host_ipv6_global_addresses);
  EXPECT_EQ(uint32_t{kIPv6PrefixLen}, mojo->host_ipv6_prefix_length);
  EXPECT_EQ(kIPv6Gateway, mojo->host_ipv6_gateway);
  EXPECT_EQ(
      std::vector<std::string>({kNameServer1, kNameServer2, kNameServerIPv6}),
      mojo->host_dns_addresses);
  EXPECT_EQ(std::vector<std::string>({kDomainName}), mojo->host_search_domains);
  EXPECT_EQ(uint32_t{kHostMtu}, mojo->host_mtu);
  EXPECT_EQ(std::vector<std::string>({kIncludedRoute}), mojo->include_routes);
  EXPECT_EQ(std::vector<std::string>({kExcludedRoute}), mojo->exclude_routes);

  EXPECT_FALSE(mojo->wifi.is_null());
  EXPECT_EQ(kBssid, mojo->wifi->bssid);
  EXPECT_EQ(kHexSsid, mojo->wifi->hex_ssid);
  EXPECT_EQ(arc::mojom::SecurityType::WPA_PSK, mojo->wifi->security);
  EXPECT_EQ(kFrequency, mojo->wifi->frequency);
  EXPECT_EQ(kSignalStrength, mojo->wifi->signal_strength);
}

TEST_F(ArcNetUtilsTest, FillConfigurationsFromDevice) {
  auto mojo = arc::mojom::NetworkConfiguration::New();
  auto device =
      GetDevice(kTestWiFiDeviceInterface, kTestWiFiDeviceGuestInterface,
                patchpanel::NetworkDevice::WIFI);
  // set IP config of device.
  device.set_ipv4_addr(StringToIPv4Address(kTestWiFiAddress).s_addr);
  device.set_host_ipv4_addr(StringToIPv4Address(kIPv4Gateway).s_addr);
  device.mutable_ipv4_subnet()->set_prefix_len(kIPv4PrefixLen);
  auto ipv4_addr = StringToIPv4Address(kNameServer1);
  device.set_dns_proxy_ipv4_addr(reinterpret_cast<const char*>(&ipv4_addr),
                                 sizeof(ipv4_addr));
  auto ipv6_addr = StringToIPv6Address(kNameServerIPv6);
  device.set_dns_proxy_ipv6_addr(reinterpret_cast<const char*>(&ipv6_addr),
                                 sizeof(ipv6_addr));
  net_utils::FillConfigurationsFromDevice(device, mojo.get());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTestWiFiDeviceGuestInterface, mojo->arc_network_interface);
  EXPECT_EQ(kTestWiFiAddress, mojo->arc_ipv4_address);
  EXPECT_EQ(kIPv4Gateway, mojo->arc_ipv4_gateway);
  EXPECT_EQ((uint32_t)kIPv4PrefixLen, mojo->arc_ipv4_prefix_length);
  EXPECT_EQ(kNameServer1, mojo->dns_proxy_addresses.value()[0]);
  EXPECT_EQ(kNameServerIPv6, mojo->dns_proxy_addresses.value()[1]);
}

TEST_F(ArcNetUtilsTest, TranslateNetworkDevices) {
  std::vector<patchpanel::NetworkDevice> network_devices;

  network_devices.emplace_back(GetDevice(kTestEthDeviceInterface,
                                         kTestEthDeviceGuestInterface,
                                         patchpanel::NetworkDevice::ETHERNET));
  network_devices.emplace_back(GetDevice(kTestWiFiDeviceInterface,
                                         kTestWiFiDeviceGuestInterface,
                                         patchpanel::NetworkDevice::WIFI));

  std::map<std::string, base::Value::Dict> shill_network_properties;
  shill_network_properties[kNetworkStatePath] = GetShillDict();

  ash::NetworkStateHandler::NetworkStateList network_states;
  network_states.push_back(GetNetworkState());
  std::vector<arc::mojom::NetworkConfigurationPtr> res =
      net_utils::TranslateNetworkDevices(network_devices, /*arc_vpn_path=*/"",
                                         network_states,
                                         shill_network_properties);
  base::RunLoop().RunUntilIdle();

  // In TranslateNetworkDevices, A network devices without associated state is
  // reported, so both the ethernet device and the wifi device are available.

  EXPECT_EQ(2u, res.size());
  for (const arc::mojom::NetworkConfigurationPtr& mojo : res) {
    if (mojo->network_interface.value() == kTestWiFiDeviceInterface) {
      EXPECT_EQ(kTestWiFiDeviceGuestInterface, mojo->arc_network_interface);
      EXPECT_EQ(mojom::NetworkType::WIFI, mojo->type);
      EXPECT_EQ(kGuid, mojo->guid);
    } else if (mojo->network_interface.value() == kTestEthDeviceInterface) {
      EXPECT_EQ(kTestEthDeviceGuestInterface, mojo->arc_network_interface);
      EXPECT_EQ(mojom::NetworkType::ETHERNET, mojo->type);
      EXPECT_EQ(0u, mojo->guid.length());
    } else {
      GTEST_FAIL() << "Unknown network interface "
                   << mojo->network_interface.value_or("(no ifname)");
    }
  }
}

TEST_F(ArcNetUtilsTest, TranslateNetworkStates) {
  std::map<std::string, base::Value::Dict> shill_network_properties;
  shill_network_properties[kNetworkStatePath] = GetShillDict();

  ash::NetworkStateHandler::NetworkStateList network_states;
  network_states.push_back(GetNetworkState());
  std::vector<arc::mojom::NetworkConfigurationPtr> res =
      net_utils::TranslateNetworkStates(/*arc_vpn_path=*/"", network_states,
                                        shill_network_properties);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, network_states.size());
  EXPECT_EQ(1u, res.size());
  EXPECT_EQ(kNetworkStatePath, res[0]->service_name);
}

TEST_F(ArcNetUtilsTest, TranslateSubjectNameMatchListToValue) {
  std::vector<std::string> subjectMatch = {
      "DNS:example1.com", "DNS:example2.com", "EMAIL:example@domain.com"};
  base::Value::List result =
      net_utils::TranslateSubjectNameMatchListToValue(subjectMatch);

  EXPECT_EQ(*result[0].GetDict().FindString(
                ::onc::eap_subject_alternative_name_match::kType),
            "DNS");
  EXPECT_EQ(*result[0].GetDict().FindString(
                ::onc::eap_subject_alternative_name_match::kValue),
            "example1.com");
  EXPECT_EQ(*result[1].GetDict().FindString(
                ::onc::eap_subject_alternative_name_match::kType),
            "DNS");
  EXPECT_EQ(*result[1].GetDict().FindString(
                ::onc::eap_subject_alternative_name_match::kValue),
            "example2.com");
  EXPECT_EQ(*result[2].GetDict().FindString(
                ::onc::eap_subject_alternative_name_match::kType),
            "EMAIL");
  EXPECT_EQ(*result[2].GetDict().FindString(
                ::onc::eap_subject_alternative_name_match::kValue),
            "example@domain.com");
}

TEST_F(ArcNetUtilsTest, TranslateSocketConnectionEvent) {
  mojom::SocketConnectionEventPtr mojom = GetMojomSocketConnectionEvent();
  const auto msg = net_utils::TranslateSocketConnectionEvent(mojom);
  EXPECT_NE(msg, nullptr);
  struct in_addr addr = {};
  addr.s_addr = kTestWiFiAddressNBO;
  EXPECT_EQ(0,
            memcmp((void*)&addr, (void*)msg->saddr().c_str(), sizeof(in_addr)));
  addr.s_addr = kDestinationAddressNBO;
  EXPECT_EQ(0,
            memcmp((void*)&addr, (void*)msg->daddr().c_str(), sizeof(in_addr)));
  EXPECT_EQ(kPort1, msg->sport());
  EXPECT_EQ(kPort2, msg->dport());
  EXPECT_EQ(patchpanel::SocketConnectionEvent::QosCategory::
                SocketConnectionEvent_QosCategory_REALTIME_INTERACTIVE,
            msg->category());
  EXPECT_EQ(patchpanel::SocketConnectionEvent::SocketEvent::
                SocketConnectionEvent_SocketEvent_OPEN,
            msg->event());
  EXPECT_EQ(patchpanel::SocketConnectionEvent::IpProtocol::
                SocketConnectionEvent_IpProtocol_TCP,
            msg->proto());
}

TEST_F(ArcNetUtilsTest, TranslateScanResults) {
  ash::NetworkStateHandler::NetworkStateList network_states;
  network_states.push_back(GetNetworkState());
  std::vector<arc::mojom::WifiScanResultPtr> res =
      net_utils::TranslateScanResults(network_states);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, res.size());
  EXPECT_EQ(kBssid, res[0]->bssid);
  EXPECT_EQ(kHexSsid, res[0]->hex_ssid);
  EXPECT_EQ(arc::mojom::SecurityType::WPA_PSK, res[0]->security);
  EXPECT_EQ(kFrequency, res[0]->frequency);
  EXPECT_EQ(kRssi, res[0]->rssi);
}

TEST_F(ArcNetUtilsTest, AreConfigurationsEquivalent) {
  std::vector<arc::mojom::NetworkConfigurationPtr> networks1;
  std::vector<arc::mojom::NetworkConfigurationPtr> networks2;

  auto network = arc::mojom::NetworkConfiguration::New();
  network->arc_network_interface = "arc0";
  network->guid = kGuid;
  network->connection_state = arc::mojom::ConnectionStateType::CONNECTED;
  network->is_default_network = true;
  network->type = arc::mojom::NetworkType::WIFI;
  network->is_metered = false;
  network->network_interface = kTestWiFiDeviceInterface;
  network->host_mtu = kHostMtu;
  network->host_search_domains = std::vector<std::string>();
  network->host_search_domains->push_back("search.domain");
  network->host_ipv4_address = kTestWiFiAddress;
  network->host_ipv6_global_addresses = std::vector<std::string>();
  network->host_ipv6_global_addresses->push_back(kNameServerIPv6);
  network->host_dns_addresses = std::vector<std::string>();
  network->host_search_domains->push_back(kNameServer1);
  network->dns_proxy_addresses = std::vector<std::string>();
  network->dns_proxy_addresses->push_back("100.115.92.134");

  // Empty vectors should be equivalent
  networks1.clear();
  networks2.clear();
  EXPECT_TRUE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // Compare one NetworkConfiguration
  networks1.clear();
  networks2.clear();
  networks1.push_back(network->Clone());
  networks2.push_back(network->Clone());
  EXPECT_TRUE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // Missing element from one of the vectors
  networks1.clear();
  networks2.clear();
  networks1.push_back(network->Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));
  networks1.clear();
  networks2.push_back(network->Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // Different order of NetworkConfigurations should be fine
  networks1.clear();
  networks2.clear();
  auto network1 = network.Clone();
  auto network2 = network.Clone();
  network1->arc_network_interface = "arc0";
  network2->arc_network_interface = "arc1";
  networks1.push_back(network1->Clone());
  networks1.push_back(network2->Clone());
  networks2.push_back(network2->Clone());
  networks2.push_back(network1->Clone());
  EXPECT_TRUE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // arc_network_interface
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->arc_network_interface = "arc0";
  network2->arc_network_interface = "arc1";
  networks1.push_back(network1->Clone());
  networks2.push_back(network2->Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // guid
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->guid = "guid1";
  network2->guid = "guid2";
  networks1.push_back(network1->Clone());
  networks2.push_back(network2->Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // connection_state
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->connection_state = arc::mojom::ConnectionStateType::CONNECTED;
  network2->connection_state = arc::mojom::ConnectionStateType::NOT_CONNECTED;
  networks1.push_back(network1.Clone());
  networks2.push_back(network2.Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // is_default_network
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->is_default_network = true;
  network2->is_default_network = false;
  networks1.push_back(network1.Clone());
  networks2.push_back(network2.Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // type
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->type = arc::mojom::NetworkType::WIFI;
  network2->type = arc::mojom::NetworkType::CELLULAR;
  networks1.push_back(network1.Clone());
  networks2.push_back(network2.Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // is_metered
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->is_metered = true;
  network2->is_metered = false;
  networks1.push_back(network1.Clone());
  networks2.push_back(network2.Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // network_interface
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->network_interface = "eth0";
  network2->network_interface = "wlan0";
  networks1.push_back(network1.Clone());
  networks2.push_back(network2.Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // host_mtu
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->host_mtu = 32;
  network2->host_mtu = 64;
  networks1.push_back(network1.Clone());
  networks2.push_back(network2.Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // host_dns_addresses
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->host_dns_addresses->push_back("1.1.1.1");
  network2->host_dns_addresses->push_back("8.8.8.8");
  networks1.push_back(network1.Clone());
  networks2.push_back(network2.Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // Mismatching order of same host_dns_addresses is still not equivalent
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->host_dns_addresses->push_back("1.1.1.1");
  network1->host_dns_addresses->push_back("8.8.8.8");
  network2->host_dns_addresses->push_back("8.8.8.8");
  network2->host_dns_addresses->push_back("1.1.1.1");
  networks1.push_back(network1.Clone());
  networks2.push_back(network2.Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // dns_proxy_addresses
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->dns_proxy_addresses->push_back("100.115.92.0");
  network2->dns_proxy_addresses->push_back("100.115.92.1");
  networks1.push_back(network1.Clone());
  networks2.push_back(network2.Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // host_search_domains
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->host_search_domains->push_back("search.domain.1");
  network2->host_search_domains->push_back("search.domain.2");
  networks1.push_back(network1.Clone());
  networks2.push_back(network2.Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // host_ipv4_address
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->host_ipv4_address = "127.0.0.1";
  network1->host_ipv4_address = "0.0.0.0";
  networks1.push_back(network1.Clone());
  networks2.push_back(network2.Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));

  // host_ipv6_global_addresses
  networks1.clear();
  networks2.clear();
  network1 = network.Clone();
  network2 = network.Clone();
  network1->host_ipv6_global_addresses->push_back("2001:db8::");
  network2->host_ipv6_global_addresses->push_back("::1234:5678");
  networks1.push_back(network1.Clone());
  networks2.push_back(network2.Clone());
  EXPECT_FALSE(net_utils::AreConfigurationsEquivalent(networks1, networks2));
}
}  // namespace
}  // namespace arc
