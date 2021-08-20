// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/networking_log.h"

#include "ash/webui/diagnostics_ui/backend/log_test_helpers.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace {

mojom::NetworkPtr CreateWiFiNetworkPtr(uint32_t signal_strength,
                                       uint32_t frequency,
                                       const std::string& ssid,
                                       const std::string& bssid,
                                       uint32_t routing_prefix,
                                       const std::string& gateway,
                                       const std::string& ip_address,
                                       std::vector<std::string>&& name_servers,
                                       const std::string& guid,
                                       const std::string& name,
                                       const std::string& mac_address,
                                       mojom::SecurityType security) {
  auto type_props = mojom::NetworkTypeProperties::New();
  auto wifi_props = mojom::WiFiStateProperties::New(signal_strength, frequency,
                                                    ssid, bssid, security);
  type_props->set_wifi(std::move(wifi_props));
  auto ip_config = mojom::IPConfigProperties::New(
      std::move(name_servers), routing_prefix, gateway, ip_address);
  return mojom::Network::New(mojom::NetworkState::kOnline,
                             mojom::NetworkType::kWiFi, std::move(type_props),
                             guid, name, mac_address, std::move(ip_config));
}

// TODO(michaelcheco): Add missing Ethernet type properties.
mojom::NetworkPtr CreateEthernetNetworkPtr(const std::string& guid,
                                           const std::string& name,
                                           const std::string& mac_address) {
  return mojom::Network::New(mojom::NetworkState::kOnline,
                             mojom::NetworkType::kEthernet,
                             mojom::NetworkTypeProperties::New(), guid, name,
                             mac_address, mojom::IPConfigProperties::New());
}

// TODO(michaelcheco): Add missing Cellular type properties.
mojom::NetworkPtr CreateCellularNetworkPtr(const std::string& guid,
                                           const std::string& name,
                                           const std::string& mac_address) {
  return mojom::Network::New(mojom::NetworkState::kOnline,
                             mojom::NetworkType::kCellular,
                             mojom::NetworkTypeProperties::New(), guid, name,
                             mac_address, mojom::IPConfigProperties::New());
}

}  // namespace

class NetworkingLogTest : public testing::Test {
 public:
  NetworkingLogTest() = default;

  ~NetworkingLogTest() override = default;
};

TEST_F(NetworkingLogTest, DetailedLogContentsWiFi) {
  const uint32_t expected_signal_strength = 99;
  const uint16_t expected_frequency = 5785;
  const std::string expected_ssid = "ssid";
  const std::string expected_bssid = "bssid";
  const std::string expected_subnet_mask = "128.0.0.0";
  const std::string expected_gateway = "192.0.0.1";
  const std::string expected_ip_address = "192.168.1.1";
  const std::string expected_security_type = "WEP";
  const std::string name_server1 = "192.168.1.100";
  const std::string name_server2 = "192.168.1.101";

  std::vector<std::string> expected_name_servers = {name_server1, name_server2};
  const std::string expected_guid = "guid";
  const std::string expected_name = "name";
  const std::string expected_mac_address = "84:C5:A6:30:3F:31";

  mojom::NetworkPtr test_info = CreateWiFiNetworkPtr(
      expected_signal_strength, expected_frequency, expected_ssid,
      expected_bssid, /*routing_prefix=*/1, expected_gateway,
      expected_ip_address, std::move(expected_name_servers), expected_guid,
      expected_name, expected_mac_address, mojom::SecurityType::kWepPsk);

  NetworkingLog log;

  log.UpdateContents(test_info.Clone());

  const std::string log_as_string = log.GetContents();
  const std::vector<std::string> log_lines = GetLogLines(log_as_string);

  // Expect one title line and 13 content lines.
  EXPECT_EQ(14u, log_lines.size());
  EXPECT_EQ("--- Networking Info ---", log_lines[0]);
  EXPECT_EQ("Name: " + expected_name, log_lines[1]);
  EXPECT_EQ("Type: WiFi", log_lines[2]);
  EXPECT_EQ("State: Online", log_lines[3]);
  EXPECT_EQ("MAC Address: " + expected_mac_address, log_lines[4]);
  EXPECT_EQ(
      "Signal Strength: " + base::NumberToString(expected_signal_strength),
      log_lines[5]);
  EXPECT_EQ("Frequency: " + base::NumberToString(expected_frequency),
            log_lines[6]);
  EXPECT_EQ("SSID: " + expected_ssid, log_lines[7]);
  EXPECT_EQ("BSSID: " + expected_bssid, log_lines[8]);
  EXPECT_EQ("Security: " + expected_security_type, log_lines[9]);
  EXPECT_EQ("Gateway: " + expected_gateway, log_lines[10]);
  EXPECT_EQ("IP Address: " + expected_ip_address, log_lines[11]);
  EXPECT_EQ("Name Servers: " + name_server1 + ", " + name_server2,
            log_lines[12]);
  EXPECT_EQ("Subnet Mask: " + expected_subnet_mask, log_lines[13]);
}

// TODO(michaelcheco): Update test when Cellular type properties are added.
TEST_F(NetworkingLogTest, DetailedLogContentsEthernet) {
  const std::string expected_guid = "guid";
  const std::string expected_name = "name";
  const std::string expected_mac_address = "84:C5:A6:30:3F:31";

  mojom::NetworkPtr test_info = CreateEthernetNetworkPtr(
      expected_guid, expected_name, expected_mac_address);

  NetworkingLog log;

  log.UpdateContents(test_info.Clone());

  const std::string log_as_string = log.GetContents();
  const std::vector<std::string> log_lines = GetLogLines(log_as_string);

  // Expect one title line and 8 content lines.
  EXPECT_EQ(9u, log_lines.size());
  EXPECT_EQ("--- Networking Info ---", log_lines[0]);
  EXPECT_EQ("Name: " + expected_name, log_lines[1]);
  EXPECT_EQ("Type: Ethernet", log_lines[2]);
  EXPECT_EQ("State: Online", log_lines[3]);
  EXPECT_EQ("MAC Address: " + expected_mac_address, log_lines[4]);
}

// TODO(michaelcheco): Update test when Ethernet type properties are added.
TEST_F(NetworkingLogTest, DetailedLogContentsCellular) {
  const std::string expected_guid = "guid";
  const std::string expected_name = "name";
  const std::string expected_mac_address = "84:C5:A6:30:3F:31";
  mojom::NetworkPtr test_info = CreateCellularNetworkPtr(
      expected_guid, expected_name, expected_mac_address);

  NetworkingLog log;

  log.UpdateContents(test_info.Clone());

  const std::string log_as_string = log.GetContents();
  const std::vector<std::string> log_lines = GetLogLines(log_as_string);

  // Expect one title line and 8 content lines.
  EXPECT_EQ(9u, log_lines.size());
  EXPECT_EQ("--- Networking Info ---", log_lines[0]);
  EXPECT_EQ("Name: " + expected_name, log_lines[1]);
  EXPECT_EQ("Type: Cellular", log_lines[2]);
  EXPECT_EQ("State: Online", log_lines[3]);
  EXPECT_EQ("MAC Address: " + expected_mac_address, log_lines[4]);
}

}  // namespace diagnostics
}  // namespace ash
