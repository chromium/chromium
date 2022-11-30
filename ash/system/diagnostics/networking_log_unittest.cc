// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/networking_log.h"

#include <vector>

#include "ash/system/diagnostics/log_test_helpers.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace {

constexpr char kNetworkInfoHeader[] = "--- Network Info ---";

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
  auto wifi_props = mojom::WiFiStateProperties::New(signal_strength, frequency,
                                                    ssid, bssid, security);
  auto type_props =
      mojom::NetworkTypeProperties::NewWifi(std::move(wifi_props));
  auto ip_config = mojom::IPConfigProperties::New(
      std::move(name_servers), routing_prefix, gateway, ip_address);
  return mojom::Network::New(mojom::NetworkState::kOnline,
                             mojom::NetworkType::kWiFi, std::move(type_props),
                             guid, name, mac_address, std::move(ip_config));
}

mojom::NetworkPtr CreateEthernetNetworkPtr(
    const std::string& guid,
    const std::string& name,
    const std::string& mac_address,
    const mojom::AuthenticationType& authentication) {
  auto ethernet_props = mojom::EthernetStateProperties::New(authentication);
  auto type_props =
      mojom::NetworkTypeProperties::NewEthernet(std::move(ethernet_props));
  return mojom::Network::New(mojom::NetworkState::kOnline,
                             mojom::NetworkType::kEthernet,
                             std::move(type_props), guid, name, mac_address,
                             mojom::IPConfigProperties::New());
}

mojom::NetworkPtr CreateCellularNetworkPtr(
    const std::string& guid,
    const std::string& name,
    const std::string& mac_address,
    const std::string& iccid,
    const std::string& eid,
    const std::string& network_technology,
    bool roaming,
    mojom::RoamingState roaming_state,
    const uint32_t signal_strength,
    bool sim_locked,
    mojom::LockType lock_type) {
  auto cellular_props = mojom::CellularStateProperties::New(
      iccid, eid, network_technology, roaming, roaming_state, signal_strength,
      sim_locked, lock_type);
  auto type_props =
      mojom::NetworkTypeProperties::NewCellular(std::move(cellular_props));
  return mojom::Network::New(mojom::NetworkState::kOnline,
                             mojom::NetworkType::kCellular,
                             std::move(type_props), guid, name, mac_address,
                             mojom::IPConfigProperties::New());
}

// Splits `line` on '-' ignoring the first part which is the date, and verifies
// that the second half of the line equals `expected_message`.
void ExpectCorrectLogLine(const std::string& expected_message,
                          const std::string& line) {
  const std::vector<std::string> event_parts = GetLogLineContents(line);
  EXPECT_EQ(2u, event_parts.size());
  EXPECT_EQ(expected_message, event_parts[1]);
}

}  // namespace

class NetworkingLogTest : public testing::Test {
 public:
  NetworkingLogTest() { EXPECT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  ~NetworkingLogTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
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

  NetworkingLog log(temp_dir_.GetPath());

  log.UpdateNetworkList({expected_guid}, expected_guid);
  log.UpdateNetworkState(test_info.Clone());
  task_environment_.RunUntilIdle();

  const std::string log_as_string = log.GetNetworkInfo();
  const std::vector<std::string> log_lines = GetLogLines(log_as_string);

  // Expect one title line and 14 content lines.
  EXPECT_EQ(15u, log_lines.size());
  EXPECT_EQ(kNetworkInfoHeader, log_lines[0]);
  EXPECT_EQ("Name: " + expected_name, log_lines[1]);
  EXPECT_EQ("Type: WiFi", log_lines[2]);
  EXPECT_EQ("State: Online", log_lines[3]);
  EXPECT_EQ("Active: True", log_lines[4]);
  EXPECT_EQ("MAC Address: " + expected_mac_address, log_lines[5]);
  EXPECT_EQ(
      "Signal Strength: " + base::NumberToString(expected_signal_strength),
      log_lines[6]);
  EXPECT_EQ("Frequency: " + base::NumberToString(expected_frequency),
            log_lines[7]);
  EXPECT_EQ("SSID: " + expected_ssid, log_lines[8]);
  EXPECT_EQ("BSSID: " + expected_bssid, log_lines[9]);
  EXPECT_EQ("Security: " + expected_security_type, log_lines[10]);
  EXPECT_EQ("Gateway: " + expected_gateway, log_lines[11]);
  EXPECT_EQ("IP Address: " + expected_ip_address, log_lines[12]);
  EXPECT_EQ("Name Servers: " + name_server1 + ", " + name_server2,
            log_lines[13]);
  EXPECT_EQ("Subnet Mask: " + expected_subnet_mask, log_lines[14]);

  // Expect one title and one event for adding the network.
  const std::string events_log = log.GetNetworkEvents();
  const std::vector<std::string> events_lines = GetLogLines(events_log);
  EXPECT_EQ(2u, events_lines.size());
  EXPECT_EQ("--- Network Events ---", events_lines[0]);

  const std::string expected_line =
      "WiFi network [" + expected_mac_address + "] started in state Online";
  ExpectCorrectLogLine(expected_line, events_lines[1]);
}

TEST_F(NetworkingLogTest, DetailedLogContentsEthernet) {
  const std::string expected_guid = "guid";
  const std::string expected_name = "name";
  const std::string expected_mac_address = "84:C5:A6:30:3F:31";
  const std::string expected_authentication = "EAP";

  mojom::NetworkPtr test_info = CreateEthernetNetworkPtr(
      expected_guid, expected_name, expected_mac_address,
      mojom::AuthenticationType::k8021x);

  NetworkingLog log(temp_dir_.GetPath());

  log.UpdateNetworkList({expected_guid}, expected_guid);
  log.UpdateNetworkState(test_info.Clone());
  task_environment_.RunUntilIdle();

  const std::string log_as_string = log.GetNetworkInfo();
  const std::vector<std::string> log_lines = GetLogLines(log_as_string);

  // Expect one title line and 10 content lines.
  EXPECT_EQ(11u, log_lines.size());
  EXPECT_EQ(kNetworkInfoHeader, log_lines[0]);
  EXPECT_EQ("Name: " + expected_name, log_lines[1]);
  EXPECT_EQ("Type: Ethernet", log_lines[2]);
  EXPECT_EQ("State: Online", log_lines[3]);
  EXPECT_EQ("Active: True", log_lines[4]);
  EXPECT_EQ("MAC Address: " + expected_mac_address, log_lines[5]);
  EXPECT_EQ("Authentication: " + expected_authentication, log_lines[6]);

  // Expect one title and one event for adding the network.
  const std::string events_log = log.GetNetworkEvents();
  const std::vector<std::string> events_lines = GetLogLines(events_log);
  EXPECT_EQ(2u, events_lines.size());
  EXPECT_EQ("--- Network Events ---", events_lines[0]);

  const std::string expected_line =
      "Ethernet network [" + expected_mac_address + "] started in state Online";
  ExpectCorrectLogLine(expected_line, events_lines[1]);
}

TEST_F(NetworkingLogTest, DetailedLogContentsCellular) {
  const std::string expected_guid = "guid";
  const std::string expected_name = "name";
  const std::string expected_mac_address = "84:C5:A6:30:3F:31";
  const std::string expected_iccid = "83948080007483825411";
  const std::string expected_eid = "82099038007008862600508229159883";
  const std::string expected_network_technology = "LTE";
  const std::string expected_roaming = "False";
  const std::string expected_roaming_state = "None";
  const uint32_t expected_signal_strength = 89;
  const std::string expected_sim_locked = "True";
  const std::string expected_lock_type = "sim-pin";

  mojom::NetworkPtr test_info = CreateCellularNetworkPtr(
      expected_guid, expected_name, expected_mac_address, expected_iccid,
      expected_eid, expected_network_technology, false,
      mojom::RoamingState::kNone, expected_signal_strength, true,
      mojom::LockType::kSimPin);

  NetworkingLog log(temp_dir_.GetPath());

  log.UpdateNetworkList({expected_guid}, expected_guid);
  log.UpdateNetworkState(test_info.Clone());
  task_environment_.RunUntilIdle();

  const std::string log_as_string = log.GetNetworkInfo();
  const std::vector<std::string> log_lines = GetLogLines(log_as_string);

  // Expect one title line and 17 content lines.
  EXPECT_EQ(18u, log_lines.size());
  EXPECT_EQ(kNetworkInfoHeader, log_lines[0]);
  EXPECT_EQ("Name: " + expected_name, log_lines[1]);
  EXPECT_EQ("Type: Cellular", log_lines[2]);
  EXPECT_EQ("State: Online", log_lines[3]);
  EXPECT_EQ("Active: True", log_lines[4]);
  EXPECT_EQ("MAC Address: " + expected_mac_address, log_lines[5]);
  EXPECT_EQ("ICCID: " + expected_iccid, log_lines[6]);
  EXPECT_EQ("EID: " + expected_eid, log_lines[7]);
  EXPECT_EQ("Technology: " + expected_network_technology, log_lines[8]);
  EXPECT_EQ("Roaming: " + expected_roaming, log_lines[9]);
  EXPECT_EQ("Roaming State: " + expected_roaming_state, log_lines[10]);
  EXPECT_EQ(
      "Signal Strength: " + base::NumberToString(expected_signal_strength),
      log_lines[11]);
  EXPECT_EQ("SIM Locked: " + expected_sim_locked, log_lines[12]);
  EXPECT_EQ("SIM Lock Type: " + expected_lock_type, log_lines[13]);

  // Expect one title and one event for adding the network.
  const std::string events_log = log.GetNetworkEvents();
  const std::vector<std::string> events_lines = GetLogLines(events_log);
  EXPECT_EQ(2u, events_lines.size());
  EXPECT_EQ("--- Network Events ---", events_lines[0]);

  const std::string expected_line =
      "Cellular network [" + expected_mac_address + "] started in state Online";
  ExpectCorrectLogLine(expected_line, events_lines[1]);
}

TEST_F(NetworkingLogTest, NetworkEvents) {
  const std::string expected_guid = "guid";
  const std::string expected_name = "name";
  const std::string expected_mac_address = "84:C5:A6:30:3F:31";

  mojom::NetworkPtr test_info = CreateEthernetNetworkPtr(
      expected_guid, expected_name, expected_mac_address,
      mojom::AuthenticationType::k8021x);

  NetworkingLog log(temp_dir_.GetPath());

  // Add the network.
  log.UpdateNetworkList({expected_guid}, expected_guid);
  log.UpdateNetworkState(test_info.Clone());

  // Change the state of the network from Online to Disabled.
  mojom::NetworkPtr new_state = test_info.Clone();
  new_state->state = mojom::NetworkState::kDisabled;
  log.UpdateNetworkState(std::move(new_state));

  // Remove the network.
  log.UpdateNetworkList({}, "expected_guid");

  // Make sure all the updates are committed.
  task_environment_.RunUntilIdle();

  // Split the log for verification.
  const std::string events_log = log.GetNetworkEvents();
  const std::vector<std::string> events_lines = GetLogLines(events_log);
  EXPECT_EQ(4u, events_lines.size());

  // Verify section header.
  size_t upto_line = 0;
  EXPECT_EQ("--- Network Events ---", events_lines[upto_line++]);

  // Verify add event.
  std::string expected_line =
      "Ethernet network [" + expected_mac_address + "] started in state Online";
  ExpectCorrectLogLine(expected_line, events_lines[upto_line++]);

  // Verify state change event.
  expected_line = "Ethernet network [" + expected_mac_address +
                  "] changed state from Online to Disabled";
  ExpectCorrectLogLine(expected_line, events_lines[upto_line++]);

  // Verify remove event.
  expected_line = "Ethernet network [" + expected_mac_address + "] removed";
  ExpectCorrectLogLine(expected_line, events_lines[upto_line++]);
}

TEST_F(NetworkingLogTest, WiFiNetworkEvents) {
  const uint32_t expected_signal_strength = 99;
  const uint16_t expected_frequency = 5785;
  const std::string expected_ssid = "ssid";
  const std::string expected_bssid = "bssid";
  const std::string expected_bssid_roamed = "bssid_roamed";
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

  NetworkingLog log(temp_dir_.GetPath());

  // Add the network.
  log.UpdateNetworkList({expected_guid}, expected_guid);
  log.UpdateNetworkState(test_info.Clone());

  // Leave the WiFi network.
  mojom::NetworkPtr new_state = test_info.Clone();
  new_state->state = mojom::NetworkState::kNotConnected;
  new_state->type_properties->get_wifi()->ssid = "";
  log.UpdateNetworkState(std::move(new_state));

  // Rejoin the WiFi network.
  new_state = test_info.Clone();
  new_state->state = mojom::NetworkState::kOnline;
  new_state->type_properties->get_wifi()->ssid = expected_ssid;
  log.UpdateNetworkState(std::move(new_state));

  // Roam to a new access point.
  new_state = test_info.Clone();
  new_state->state = mojom::NetworkState::kOnline;
  new_state->type_properties->get_wifi()->bssid = expected_bssid_roamed;
  log.UpdateNetworkState(std::move(new_state));

  // Remove the network.
  log.UpdateNetworkList({}, "expected_guid");

  // Make sure all the updates are committed.
  task_environment_.RunUntilIdle();

  // Split the log for verification.
  const std::string events_log = log.GetNetworkEvents();
  const std::vector<std::string> events_lines = GetLogLines(events_log);
  EXPECT_EQ(8u, events_lines.size());

  // Verify section header.
  size_t upto_line = 0;
  EXPECT_EQ("--- Network Events ---", events_lines[upto_line++]);

  // Verify add event.
  std::string expected_line =
      "WiFi network [" + expected_mac_address + "] started in state Online";
  ExpectCorrectLogLine(expected_line, events_lines[upto_line++]);

  // Verify network leave event.
  expected_line = "WiFi network [" + expected_mac_address + "] left SSID '" +
                  expected_ssid + "'";
  ExpectCorrectLogLine(expected_line, events_lines[upto_line++]);

  // Verify state change event.
  expected_line = "WiFi network [" + expected_mac_address +
                  "] changed state from Online to Not Connected";
  ExpectCorrectLogLine(expected_line, events_lines[upto_line++]);

  // Verify network join event.
  expected_line = "WiFi network [" + expected_mac_address + "] joined SSID '" +
                  expected_ssid + "' on access point [" + expected_bssid + "]";
  ExpectCorrectLogLine(expected_line, events_lines[upto_line++]);

  // Verify state change event.
  expected_line = "WiFi network [" + expected_mac_address +
                  "] changed state from Not Connected to Online";
  ExpectCorrectLogLine(expected_line, events_lines[upto_line++]);

  // Verify access point roam event.
  expected_line = "WiFi network [" + expected_mac_address + "] on SSID '" +
                  expected_ssid + "' roamed from access point [" +
                  expected_bssid + "] to [" + expected_bssid_roamed + "]";
  ExpectCorrectLogLine(expected_line, events_lines[upto_line++]);

  // Verify remove event.
  expected_line = "WiFi network [" + expected_mac_address + "] removed";
  ExpectCorrectLogLine(expected_line, events_lines[upto_line++]);
}

TEST_F(NetworkingLogTest, NetworkPtrInvalidDoesNotCrash) {
  mojom::NetworkPtr null_network;
  NetworkingLog log(temp_dir_.GetPath());
  const std::vector<std::string> observer_guids;

  EXPECT_NO_FATAL_FAILURE(
      log.UpdateNetworkList(observer_guids, /**active_guid=*/""));
  EXPECT_TRUE(null_network.is_null());
  EXPECT_NO_FATAL_FAILURE(log.UpdateNetworkState(std::move(null_network)));

  // Ensure AsyncLog tasks complete.
  task_environment_.RunUntilIdle();

  // No networks, only header should be logged.
  const std::vector<std::string> logged_network_info_1 =
      ash::diagnostics::GetLogLines(log.GetNetworkInfo());
  EXPECT_EQ(1u, logged_network_info_1.size());
  EXPECT_EQ(kNetworkInfoHeader, logged_network_info_1[0]);

  // LogRemoveNetwork should not crash if NetworkPtr null.
  mojom::NetworkPtr removed_network =
      CreateEthernetNetworkPtr("fake_guid", "eth0", "00:AA:11:BB:22:CC",
                               mojom::AuthenticationType::kNone);
  EXPECT_NO_FATAL_FAILURE(log.UpdateNetworkState(std::move(removed_network)));
  EXPECT_NO_FATAL_FAILURE(
      log.UpdateNetworkList(observer_guids, /**active_guid=*/""));

  // Ensure AsyncLog tasks complete.
  task_environment_.RunUntilIdle();

  // No networks, only header should be logged.
  const std::vector<std::string> logged_network_info_2 =
      ash::diagnostics::GetLogLines(log.GetNetworkInfo());
  EXPECT_EQ(1u, logged_network_info_2.size());
  EXPECT_EQ(kNetworkInfoHeader, logged_network_info_2[0]);
}

}  // namespace diagnostics
}  // namespace ash
