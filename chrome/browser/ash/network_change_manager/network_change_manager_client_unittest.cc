// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/network_change_manager/network_change_manager_client.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/strings/string_split.h"
#include "base/value_iterators.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_passive.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kDnsServers1[] = "192.168.0.1,192.168.0.2";
const char kDnsServers2[] = "192.168.3.1,192.168.3.2";
const char kIpAddress1[] = "192.168.1.1";
const char kIpAddress2[] = "192.168.1.2";
const char kService1[] = "/service/1";
const char kService2[] = "/service/2";
const char kService3[] = "/service/3";

struct NotifierState {
  net::NetworkChangeNotifier::ConnectionType type;
  net::NetworkChangeNotifier::ConnectionSubtype subtype;
  const char* service_path;
  const char* ip_address;
  const char* dns_servers;
};

struct DefaultNetworkState {
  bool is_connected;
  const char* type;
  const char* network_technology;
  const char* service_path;
  const char* ip_address;
  const char* dns_servers;
};

struct NotifierUpdateTestCase {
  const char* test_description;
  NotifierState initial_state;
  DefaultNetworkState default_network_state;
  NotifierState expected_state;
  bool expected_type_changed;
  bool expected_subtype_changed;
  bool expected_ip_changed;
  bool expected_dns_changed;
};

}  // namespace

using net::NetworkChangeNotifier;

TEST(NetworkChangeManagerClientTest, ConnectionTypeFromShill) {
  struct TypeMapping {
    const char* shill_type;
    const char* technology;
    NetworkChangeNotifier::ConnectionType connection_type;
  };
  TypeMapping type_mappings[] = {
      {shill::kTypeEthernet, "", NetworkChangeNotifier::CONNECTION_ETHERNET},
      {shill::kTypeWifi, "", NetworkChangeNotifier::CONNECTION_WIFI},
      {"unknown type", "unknown technology",
       NetworkChangeNotifier::CONNECTION_UNKNOWN},
      {shill::kTypeCellular, shill::kNetworkTechnology1Xrtt,
       NetworkChangeNotifier::CONNECTION_2G},
      {shill::kTypeCellular, shill::kNetworkTechnologyGprs,
       NetworkChangeNotifier::CONNECTION_2G},
      {shill::kTypeCellular, shill::kNetworkTechnologyEdge,
       NetworkChangeNotifier::CONNECTION_2G},
      {shill::kTypeCellular, shill::kNetworkTechnologyEvdo,
       NetworkChangeNotifier::CONNECTION_3G},
      {shill::kTypeCellular, shill::kNetworkTechnologyGsm,
       NetworkChangeNotifier::CONNECTION_3G},
      {shill::kTypeCellular, shill::kNetworkTechnologyUmts,
       NetworkChangeNotifier::CONNECTION_3G},
      {shill::kTypeCellular, shill::kNetworkTechnologyHspa,
       NetworkChangeNotifier::CONNECTION_3G},
      {shill::kTypeCellular, shill::kNetworkTechnologyHspaPlus,
       NetworkChangeNotifier::CONNECTION_4G},
      {shill::kTypeCellular, shill::kNetworkTechnologyLte,
       NetworkChangeNotifier::CONNECTION_4G},
      {shill::kTypeCellular, shill::kNetworkTechnologyLteAdvanced,
       NetworkChangeNotifier::CONNECTION_4G},
      {shill::kTypeCellular, shill::kNetworkTechnology5gNr,
       NetworkChangeNotifier::CONNECTION_5G},
      {shill::kTypeCellular, "unknown technology",
       NetworkChangeNotifier::CONNECTION_2G}};

  for (size_t i = 0; i < std::size(type_mappings); ++i) {
    NetworkChangeNotifier::ConnectionType type =
        NetworkChangeManagerClient::ConnectionTypeFromShill(
            type_mappings[i].shill_type, type_mappings[i].technology);
    EXPECT_EQ(type_mappings[i].connection_type, type);
  }
}

TEST(NetworkChangeManagerClientTest,
     NetworkChangeNotifierConnectionTypeUpdated) {
  // Create a NetworkChangeNotifier with a non-NONE connection type.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifierPassive> network_change_notifier(
      static_cast<net::NetworkChangeNotifierPassive*>(
          net::NetworkChangeNotifier::CreateIfNeeded().release()));
  network_change_notifier->OnConnectionChanged(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
            net::NetworkChangeNotifier::GetConnectionType());

  // Initialize DBus clients and clear services so NetworkHandler thinks we're
  // offline.
  chromeos::PowerManagerClient::InitializeFake();
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper =
      std::make_unique<NetworkHandlerTestHelper>();
  network_handler_test_helper->service_test()->ClearServices();

  auto client = std::make_unique<NetworkChangeManagerClient>(
      network_change_notifier.get());

  // NetworkChangeManagerClient should have read the network state from DBus
  // and notified NetworkChangeNotifier that we're offline.
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_NONE,
            client->connection_type_);
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_NONE,
            net::NetworkChangeNotifier::GetConnectionType());

  client.reset();
  network_handler_test_helper.reset();
  chromeos::PowerManagerClient::Shutdown();
}

class NetworkChangeManagerClientUpdateTest : public testing::Test {
 public:
  NetworkChangeManagerClientUpdateTest(
      const NetworkChangeManagerClientUpdateTest&) = delete;
  NetworkChangeManagerClientUpdateTest& operator=(
      const NetworkChangeManagerClientUpdateTest&) = delete;

 protected:
  NetworkChangeManagerClientUpdateTest() : default_network_("") {}
  ~NetworkChangeManagerClientUpdateTest() override = default;

  void SetUp() override {
    network_change_notifier_ = net::NetworkChangeNotifier::CreateIfNeeded();
    chromeos::PowerManagerClient::InitializeFake();
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    proxy_ = std::make_unique<NetworkChangeManagerClient>(
        static_cast<net::NetworkChangeNotifierPassive*>(
            network_change_notifier_.get()));
  }

  void TearDown() override {
    proxy_.reset();
    network_handler_test_helper_.reset();
    chromeos::PowerManagerClient::Shutdown();
    network_change_notifier_.reset();
  }

  void SetNotifierState(const NotifierState& notifier_state) {
    proxy_->connection_type_ = notifier_state.type;
    proxy_->connection_subtype_ = notifier_state.subtype;
    proxy_->service_path_ = notifier_state.service_path;
    proxy_->ip_address_ = notifier_state.ip_address;
    proxy_->dns_servers_ = notifier_state.dns_servers;
  }

  void VerifyNotifierState(const NotifierState& notifier_state) {
    EXPECT_EQ(notifier_state.type, proxy_->connection_type_);
    EXPECT_EQ(notifier_state.subtype, proxy_->connection_subtype_);
    EXPECT_EQ(notifier_state.service_path, proxy_->service_path_);
    EXPECT_EQ(notifier_state.ip_address, proxy_->ip_address_);
    EXPECT_EQ(notifier_state.dns_servers, proxy_->dns_servers_);
  }

  // Sets the default network state used for notifier updates.
  void SetDefaultNetworkState(
      const DefaultNetworkState& default_network_state) {
    default_network_.set_visible(true);
    default_network_.set_connection_state_for_testing(
        default_network_state.is_connected ? shill::kStateOnline
                                           : shill::kStateConfiguration);
    default_network_.set_type_for_testing(default_network_state.type);
    default_network_.set_network_technology_for_testing(
        default_network_state.network_technology);
    default_network_.set_path_for_testing(default_network_state.service_path);
    base::Value::Dict ipv4_properties;
    ipv4_properties.Set(shill::kAddressProperty,
                        base::Value(default_network_state.ip_address));
    std::vector<std::string> dns_servers =
        base::SplitString(default_network_state.dns_servers, ",",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    base::Value::List dns_servers_value;
    for (const std::string& server : dns_servers)
      dns_servers_value.Append(server);

    ipv4_properties.Set(shill::kNameServersProperty,
                        std::move(dns_servers_value));
    default_network_.IPConfigPropertiesChanged(std::move(ipv4_properties));
  }

  // Process an default network update based on the state of |default_network_|.
  void ProcessDefaultNetworkUpdate(bool* dns_changed,
                                   bool* ip_changed,
                                   bool* type_changed,
                                   bool* subtype_changed) {
    proxy_->UpdateState(&default_network_, dns_changed, ip_changed,
                        type_changed, subtype_changed);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  NetworkState default_network_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<NetworkChangeManagerClient> proxy_;
};

NotifierUpdateTestCase test_cases[] = {
    {"Online -> Offline",
     {NetworkChangeNotifier::CONNECTION_ETHERNET,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      kService1,
      kIpAddress1,
      kDnsServers1},
     {false, shill::kTypeEthernet, "", kService1, "", ""},
     {NetworkChangeNotifier::CONNECTION_NONE,
      NetworkChangeNotifier::SUBTYPE_NONE,
      "",
      "",
      ""},
     true,
     true,
     true,
     true},
    {"Offline -> Offline",
     {NetworkChangeNotifier::CONNECTION_NONE,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      "",
      "",
      ""},
     {false, shill::kTypeEthernet, "", kService1, kIpAddress1, kDnsServers1},
     {NetworkChangeNotifier::CONNECTION_NONE,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      "",
      "",
      ""},
     false,
     false,
     false,
     false},
    {"Offline -> Online",
     {NetworkChangeNotifier::CONNECTION_NONE,
      NetworkChangeNotifier::SUBTYPE_NONE,
      "",
      "",
      ""},
     {true, shill::kTypeEthernet, "", kService1, kIpAddress1, kDnsServers1},
     {NetworkChangeNotifier::CONNECTION_ETHERNET,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      kService1,
      kIpAddress1,
      kDnsServers1},
     true,
     true,
     true,
     true},
    {"Online -> Online (new default service, different connection type)",
     {NetworkChangeNotifier::CONNECTION_ETHERNET,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      kService1,
      kIpAddress1,
      kDnsServers1},
     {true, shill::kTypeWifi, "", kService2, kIpAddress1, kDnsServers1},
     {NetworkChangeNotifier::CONNECTION_WIFI,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      kService2,
      kIpAddress1,
      kDnsServers1},
     true,
     false,
     true,
     true},
    {"Online -> Online (new default service, same connection type)",
     {NetworkChangeNotifier::CONNECTION_WIFI,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      kService2,
      kIpAddress1,
      kDnsServers1},
     {true, shill::kTypeWifi, "", kService3, kIpAddress1, kDnsServers1},
     {NetworkChangeNotifier::CONNECTION_WIFI,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      kService3,
      kIpAddress1,
      kDnsServers1},
     false,
     false,
     true,
     true},
    {"Online -> Online (same default service, first IP address update)",
     {NetworkChangeNotifier::CONNECTION_WIFI,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      kService3,
      "",
      kDnsServers1},
     {true, shill::kTypeWifi, "", kService3, kIpAddress2, kDnsServers1},
     {NetworkChangeNotifier::CONNECTION_WIFI,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      kService3,
      kIpAddress2,
      kDnsServers1},
     false,
     false,
     false,
     false},
    {"Online -> Online (same default service, new IP address, same DNS)",
     {NetworkChangeNotifier::CONNECTION_WIFI,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      kService3,
      kIpAddress1,
      kDnsServers1},
     {true, shill::kTypeWifi, "", kService3, kIpAddress2, kDnsServers1},
     {NetworkChangeNotifier::CONNECTION_WIFI,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      kService3,
      kIpAddress2,
      kDnsServers1},
     false,
     false,
     true,
     false},
    {"Online -> Online (same default service, same IP address, new DNS)",
     {NetworkChangeNotifier::CONNECTION_WIFI,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      kService3,
      kIpAddress2,
      kDnsServers1},
     {true, shill::kTypeWifi, "", kService3, kIpAddress2, kDnsServers2},
     {NetworkChangeNotifier::CONNECTION_WIFI,
      NetworkChangeNotifier::SUBTYPE_UNKNOWN,
      kService3,
      kIpAddress2,
      kDnsServers2},
     false,
     false,
     false,
     true},
    {"Online -> Online (change of technology but not connection type)",
     {NetworkChangeNotifier::CONNECTION_3G,
      NetworkChangeNotifier::SUBTYPE_EVDO_REV_0,
      kService3,
      kIpAddress2,
      kDnsServers1},
     {true,
      shill::kTypeCellular,
      shill::kNetworkTechnologyHspa,
      kService3,
      kIpAddress2,
      kDnsServers1},
     {NetworkChangeNotifier::CONNECTION_3G,
      NetworkChangeNotifier::SUBTYPE_HSPA,
      kService3,
      kIpAddress2,
      kDnsServers1},
     false,
     true,
     false,
     false},
    {"Online -> Online (change of technology and connection type)",
     {NetworkChangeNotifier::CONNECTION_3G,
      NetworkChangeNotifier::SUBTYPE_EVDO_REV_0,
      kService3,
      kIpAddress2,
      kDnsServers1},
     {true,
      shill::kTypeCellular,
      shill::kNetworkTechnologyLte,
      kService3,
      kIpAddress2,
      kDnsServers1},
     {NetworkChangeNotifier::CONNECTION_4G,
      NetworkChangeNotifier::SUBTYPE_LTE,
      kService3,
      kIpAddress2,
      kDnsServers1},
     true,
     true,
     false,
     false}};

TEST_F(NetworkChangeManagerClientUpdateTest, UpdateDefaultNetwork) {
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(test_cases[i].test_description);
    SetNotifierState(test_cases[i].initial_state);
    SetDefaultNetworkState(test_cases[i].default_network_state);
    bool dns_changed = false, ip_changed = false, type_changed = false,
         subtype_changed = false;
    ProcessDefaultNetworkUpdate(&dns_changed, &ip_changed, &type_changed,
                                &subtype_changed);
    VerifyNotifierState(test_cases[i].expected_state);
    EXPECT_EQ(test_cases[i].expected_dns_changed, dns_changed);
    EXPECT_EQ(test_cases[i].expected_ip_changed, ip_changed);
    EXPECT_EQ(test_cases[i].expected_type_changed, type_changed);
    EXPECT_EQ(test_cases[i].expected_subtype_changed, subtype_changed);
  }
}

}  // namespace ash
