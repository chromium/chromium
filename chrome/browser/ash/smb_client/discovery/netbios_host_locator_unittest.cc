// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/discovery/netbios_host_locator.h"

#include "base/functional/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/smb_client/discovery/fake_netbios_client.h"
#include "chrome/browser/ash/smb_client/smb_constants.h"
#include "chromeos/ash/components/dbus/smbprovider/fake_smb_provider_client.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::smb_client {
namespace {

// Helper method to create a NetworkInterface for testing.
net::NetworkInterface CreateNetworkInterface(
    const net::IPAddress& address,
    uint32_t prefix_length,
    net::NetworkChangeNotifier::ConnectionType type) {
  net::NetworkInterface interface;
  interface.address = address;
  interface.prefix_length = prefix_length;
  interface.type = type;
  return interface;
}

net::NetworkInterface CreateValidInterface() {
  return CreateNetworkInterface(net::IPAddress::IPv4Localhost(), 0,
                                net::NetworkChangeNotifier::CONNECTION_WIFI);
}

net::NetworkInterface GenerateInvalidInterface() {
  return CreateNetworkInterface(net::IPAddress::IPv4Localhost(), 0,
                                net::NetworkChangeNotifier::CONNECTION_3G);
}

net::NetworkInterfaceList GenerateInvalidInterfaceList(size_t num) {
  return net::NetworkInterfaceList(num, GenerateInvalidInterface());
}

void ExpectNoResults(bool success, const HostMap& hosts) {
  EXPECT_TRUE(success);
  EXPECT_TRUE(hosts.empty());
}

void ExpectResultsEqual(const HostMap& expected,
                        bool success,
                        const HostMap& actual) {
  EXPECT_TRUE(success);
  EXPECT_EQ(expected, actual);
}

void ExpectFailure(bool success, const HostMap& hosts) {
  EXPECT_FALSE(success);
  EXPECT_TRUE(hosts.empty());
}

}  // namespace

class NetBiosHostLocatorTest : public testing::Test {
 public:
  NetBiosHostLocatorTest() {
    // Fake SmbProviderClient that simulates the parsing of NetBios response
    // packets.
    fake_provider_client_ = std::make_unique<FakeSmbProviderClient>();
    // Bound function to get interfaces. For testing we generate a valid
    // interface for each NetBiosClient in the test so that the correct number
    // of NetBiosClients are created.
    get_interfaces_ =
        base::BindRepeating(&NetBiosHostLocatorTest::GenerateValidInterfaces,
                            base::Unretained(this));
    // NetBiosClientFactory. Creates a FakeNetBiosClient using the fake response
    // data in |clients_data_|, if any exists.
    client_factory_ = base::BindRepeating(
        &NetBiosHostLocatorTest::NetBiosClientFactory, base::Unretained(this));

    // Set up taskrunner and timer.
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    timer_ =
        std::make_unique<base::OneShotTimer>(task_runner_->GetMockTickClock());
    timer_->SetTaskRunner(task_runner_);

    // Callback used for setting |has_returned_|.
    set_true_on_returned_callback_ = base::BindOnce(
        &NetBiosHostLocatorTest::SetTrueOnReturned, base::Unretained(this));
  }

  NetBiosHostLocatorTest(const NetBiosHostLocatorTest&) = delete;
  NetBiosHostLocatorTest& operator=(const NetBiosHostLocatorTest&) = delete;

  ~NetBiosHostLocatorTest() override = default;

 protected:
  using Packet = std::vector<uint8_t>;
  using Hostnames = std::vector<std::string>;

  // Adds the data for one NetBiosClient. The NetBiosClient will return an
  // <IPEndpoint, packet>, for each response and this packet can be parsed by
  // FakeSmbProviderClient returning the hostnames. A |packet_id| is used to
  // correlate the packet that will be returned by the FakeNetBiosClient and
  // the Hostnames that the FakeSmbProviderClient should return.
  void AddNetBiosClient(const std::map<net::IPEndPoint, Hostnames>& responses) {
    std::map<net::IPEndPoint, Packet> client_data;
    for (const auto& kv : responses) {
      client_data[kv.first] = Packet{packet_id_};
      fake_provider_client_->AddNetBiosPacketParsingForTesting(packet_id_,
                                                               kv.second);
      ++packet_id_;
    }
    clients_data_.push_back(std::move(client_data));
  }

  bool has_returned_ = false;
  FindHostsCallback set_true_on_returned_callback_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<base::OneShotTimer> timer_;
  NetBiosHostLocator::GetInterfacesFunction get_interfaces_;
  NetBiosHostLocator::NetBiosClientFactory client_factory_;
  std::unique_ptr<FakeSmbProviderClient> fake_provider_client_;
  std::unique_ptr<NetBiosHostLocator> host_locator_;

 private:
  // Creates valid interfaces such that there is one valid interface for each
  // NetBios Client that needs to be created.
  net::NetworkInterfaceList GenerateValidInterfaces() {
    return net::NetworkInterfaceList(clients_data_.size(),
                                     CreateValidInterface());
  }

  // Factory Function for FakeNetBiosClient. Returns a pointer to a
  // FakeNetBiosClient preloaded with corresponding data from |clients_data|.
  std::unique_ptr<NetBiosClientInterface> NetBiosClientFactory() {
    if (current_client < clients_data_.size()) {
      return std::make_unique<FakeNetBiosClient>(
          clients_data_[current_client++]);
    }
    return std::make_unique<FakeNetBiosClient>();
  }

  // Callback for FindHosts that sets |has_returned_| to true when called.
  void SetTrueOnReturned(bool success, const HostMap& results) {
    DCHECK(!has_returned_);
    has_returned_ = true;
  }

  uint8_t packet_id_ = 0;
  uint8_t current_client = 0;
  // Each entry in the vector represents on Netbios Client that can be created.
  // Each entry in the map represents an IP, packet pair that should be returned
  // by that NetBiosClient.
  std::vector<std::map<net::IPEndPoint, Packet>> clients_data_;
};

// Calculate broadcast address correctly calculates the broadcast address
// of a NetworkInterface.
TEST_F(NetBiosHostLocatorTest, CalculateBroadcastAddress) {
  const net::NetworkInterface interface = CreateNetworkInterface(
      net::IPAddress(192, 168, 50, 152), 24 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  EXPECT_EQ(net::IPAddress(192, 168, 50, 255),
            CalculateBroadcastAddress(interface));
}

// ShouldUseInterface returns true for Wifi and Ethernet interfaces but false
// for other types of interfaces.
TEST_F(NetBiosHostLocatorTest, ShouldUseWifiAndEthernetInterfaces) {
  const net::NetworkInterface interface_wifi = CreateNetworkInterface(
      net::IPAddress::IPv4Localhost(), 24 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  const net::NetworkInterface interface_ethernet = CreateNetworkInterface(
      net::IPAddress::IPv4Localhost(), 24 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  const net::NetworkInterface interface_bluetooth = CreateNetworkInterface(
      net::IPAddress::IPv4Localhost(), 24 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_BLUETOOTH);

  // For IPv4 the max length should be 32 bits.
  const net::NetworkInterface invalid_length = CreateNetworkInterface(
      net::IPAddress::IPv4Localhost(), 40 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  EXPECT_TRUE(ShouldUseInterface(interface_wifi));
  EXPECT_TRUE(ShouldUseInterface(interface_ethernet));
  EXPECT_FALSE(ShouldUseInterface(interface_bluetooth));
  EXPECT_FALSE(ShouldUseInterface(invalid_length));
}

// ShouldUseInterface returns true for IPv4 interfaces but false for IPv6
// interfaces.
TEST_F(NetBiosHostLocatorTest, OnlyProcessIPv4Interfaces) {
  const net::NetworkInterface interface_ipv4 = CreateNetworkInterface(
      net::IPAddress::IPv4Localhost(), 24 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  const net::NetworkInterface interface_ipv6 = CreateNetworkInterface(
      net::IPAddress::IPv6Localhost(), 24 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  EXPECT_TRUE(ShouldUseInterface(interface_ipv4));
  EXPECT_FALSE(ShouldUseInterface(interface_ipv6));
}

// One interface that receives no responses properly returns no results.
TEST_F(NetBiosHostLocatorTest, OneInterfaceNoResults) {
  // Add the entry for a NetBios Client that returns no packets.
  AddNetBiosClient(std::map<net::IPEndPoint, Hostnames>());

  host_locator_ = std::make_unique<NetBiosHostLocator>(
      get_interfaces_, client_factory_, fake_provider_client_.get(),
      std::move(timer_));

  host_locator_->FindHosts(base::BindOnce(&ExpectNoResults));
  task_runner_->FastForwardBy(base::Seconds(kNetBiosDiscoveryTimeoutSeconds));
}

// Two interfaces that receive no responses properly return no results.
TEST_F(NetBiosHostLocatorTest, MultipleInterfacesNoResults) {
  // Create two NetBiosClients that don't return any packets.
  AddNetBiosClient(std::map<net::IPEndPoint, Hostnames>());
  AddNetBiosClient(std::map<net::IPEndPoint, Hostnames>());

  host_locator_ = std::make_unique<NetBiosHostLocator>(
      get_interfaces_, client_factory_, fake_provider_client_.get(),
      std::move(timer_));

  host_locator_->FindHosts(base::BindOnce(&ExpectNoResults));

  // Fast forward timer so that callback fires.
  task_runner_->FastForwardBy(base::Seconds(kNetBiosDiscoveryTimeoutSeconds));
}

// One interface that recieves responses from two different ip addresses
// returns the correct results.
TEST_F(NetBiosHostLocatorTest, OneInterfaceWithResults) {
  // Build data for a NetBiosClient
  const net::IPEndPoint source_ip_1(net::IPAddress(1, 2, 3, 4), 137);
  const Hostnames hostnames_1 = {"hostname1", "HOSTNAME_2"};

  const net::IPEndPoint source_ip_2(net::IPAddress(2, 4, 6, 8), 137);
  const Hostnames hostnames_2 = {"host.name.3"};

  std::map<net::IPEndPoint, Hostnames> netbios_client_1;
  netbios_client_1[source_ip_1] = hostnames_1;
  netbios_client_1[source_ip_2] = hostnames_2;

  // Build the map of expected results.
  HostMap expected_results;
  expected_results[hostnames_1[0]] = source_ip_1.address();
  expected_results[hostnames_1[1]] = source_ip_1.address();
  expected_results[hostnames_2[0]] = source_ip_2.address();

  // Add the entry for a NetBios Client that returns packets.
  AddNetBiosClient(netbios_client_1);

  host_locator_ = std::make_unique<NetBiosHostLocator>(
      get_interfaces_, client_factory_, fake_provider_client_.get(),
      std::move(timer_));

  host_locator_->FindHosts(
      base::BindOnce(&ExpectResultsEqual, expected_results));
  task_runner_->FastForwardBy(base::Seconds(kNetBiosDiscoveryTimeoutSeconds));
}

// Two interfaces that each receive responses from multiple ip addresses
// correctly returns results.
TEST_F(NetBiosHostLocatorTest, MultipleInterfacesWithResults) {
  // Build data for the first NetBiosClient
  const net::IPEndPoint source_ip_1(net::IPAddress(1, 2, 3, 4), 137);
  const Hostnames hostnames_1 = {"hostname1", "HOSTNAME_2"};

  const net::IPEndPoint source_ip_2(net::IPAddress(2, 4, 6, 8), 137);
  const Hostnames hostnames_2 = {"host.name.3"};

  std::map<net::IPEndPoint, Hostnames> netbios_client_1;
  netbios_client_1[source_ip_1] = hostnames_1;
  netbios_client_1[source_ip_2] = hostnames_2;

  // Build data for the second NetBiosClient
  const net::IPEndPoint source_ip_3(net::IPAddress(1, 3, 5, 9), 137);
  const Hostnames hostnames_3 = {"host name 4"};

  const net::IPEndPoint source_ip_4(net::IPAddress(2, 4, 8, 16), 137);
  const Hostnames hostnames_4 = {"hOsTnAmE-5"};

  std::map<net::IPEndPoint, Hostnames> netbios_client_2;
  netbios_client_2[source_ip_3] = hostnames_3;
  netbios_client_2[source_ip_4] = hostnames_4;

  // Build the map of expected results.
  HostMap expected_results;
  expected_results[hostnames_1[0]] = source_ip_1.address();
  expected_results[hostnames_1[1]] = source_ip_1.address();
  expected_results[hostnames_2[0]] = source_ip_2.address();
  expected_results[hostnames_3[0]] = source_ip_3.address();
  expected_results[hostnames_4[0]] = source_ip_4.address();

  // Add the entry for a NetBios Clients that return packets.
  AddNetBiosClient(netbios_client_1);
  AddNetBiosClient(netbios_client_2);

  host_locator_ = std::make_unique<NetBiosHostLocator>(
      get_interfaces_, client_factory_, fake_provider_client_.get(),
      std::move(timer_));

  host_locator_->FindHosts(
      base::BindOnce(&ExpectResultsEqual, expected_results));
  task_runner_->FastForwardBy(base::Seconds(kNetBiosDiscoveryTimeoutSeconds));
}

// Results are not duplicated when multiple interfaces receive the same response
// from one source.
TEST_F(NetBiosHostLocatorTest, MultipleInterfacesWithDuplicateResults) {
  // Build data for the first NetBiosClient.
  const net::IPEndPoint source_ip_1(net::IPAddress(1, 2, 3, 4), 137);
  const Hostnames hostnames_1 = {"hostname1", "HOSTNAME_2"};

  const net::IPEndPoint source_ip_2(net::IPAddress(2, 4, 6, 8), 137);
  const Hostnames hostnames_2 = {"host.name.3"};

  std::map<net::IPEndPoint, Hostnames> netbios_client_1;
  netbios_client_1[source_ip_1] = hostnames_1;
  netbios_client_1[source_ip_2] = hostnames_2;

  // Build data for the second NetBiosClient which also recieves the response
  // from |source_ip_2|.
  const net::IPEndPoint source_ip_4(net::IPAddress(2, 4, 8, 16), 137);
  const Hostnames hostnames_4 = {"hOsTnAmE-5"};

  std::map<net::IPEndPoint, Hostnames> netbios_client_2;
  netbios_client_2[source_ip_2] = hostnames_2;
  netbios_client_2[source_ip_4] = hostnames_4;

  // Build the map of expected results.
  HostMap expected_results;
  expected_results[hostnames_1[0]] = source_ip_1.address();
  expected_results[hostnames_1[1]] = source_ip_1.address();
  expected_results[hostnames_2[0]] = source_ip_2.address();
  expected_results[hostnames_4[0]] = source_ip_4.address();

  // Add the entry for a NetBios Clients that return packets.
  AddNetBiosClient(netbios_client_1);
  AddNetBiosClient(netbios_client_2);

  host_locator_ = std::make_unique<NetBiosHostLocator>(
      get_interfaces_, client_factory_, fake_provider_client_.get(),
      std::move(timer_));

  host_locator_->FindHosts(
      base::BindOnce(&ExpectResultsEqual, expected_results));
  task_runner_->FastForwardBy(base::Seconds(kNetBiosDiscoveryTimeoutSeconds));
}

TEST_F(NetBiosHostLocatorTest, ResultsNotReturnedUntilTimer) {
  // Build data for a NetBiosClient
  const net::IPEndPoint source_ip_1(net::IPAddress(1, 2, 3, 4), 137);
  const Hostnames hostnames_1 = {"hostname1", "HOSTNAME_2"};

  const net::IPEndPoint source_ip_2(net::IPAddress(2, 4, 6, 8), 137);
  const Hostnames hostnames_2 = {"host.name.3"};

  std::map<net::IPEndPoint, Hostnames> netbios_client_1;
  netbios_client_1[source_ip_1] = hostnames_1;
  netbios_client_1[source_ip_2] = hostnames_2;

  // Build the map of expected results.
  HostMap expected_results;
  expected_results[hostnames_1[0]] = source_ip_1.address();
  expected_results[hostnames_1[1]] = source_ip_1.address();
  expected_results[hostnames_2[0]] = source_ip_2.address();

  // Add the entry for a NetBios Client that returns packets.
  AddNetBiosClient(netbios_client_1);

  host_locator_ = std::make_unique<NetBiosHostLocator>(
      get_interfaces_, client_factory_, fake_provider_client_.get(),
      std::move(timer_));

  host_locator_->FindHosts(std::move(set_true_on_returned_callback_));
  task_runner_->FastForwardBy(base::Seconds(kNetBiosDiscoveryTimeoutSeconds) -
                              base::Milliseconds(1));
  EXPECT_FALSE(has_returned_);
  task_runner_->FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(has_returned_);
}

TEST_F(NetBiosHostLocatorTest, NoValidInterfacesReturnsNoResults) {
  auto get_invalid_interfaces_ =
      base::BindRepeating(&GenerateInvalidInterfaceList, 3 /* num */);

  host_locator_ = std::make_unique<NetBiosHostLocator>(
      get_invalid_interfaces_, client_factory_, fake_provider_client_.get(),
      std::move(timer_));

  host_locator_->FindHosts(base::BindOnce(&ExpectFailure));
}

TEST_F(NetBiosHostLocatorTest, SecondIPUsedForResults) {
  const std::string duplicate_hostname = "duplicate";

  // Build data for the first NetBiosClient.
  const net::IPEndPoint source_ip_1(net::IPAddress(1, 2, 3, 4), 137);
  const Hostnames hostnames_1 = {duplicate_hostname};

  std::map<net::IPEndPoint, Hostnames> netbios_client_1;
  netbios_client_1[source_ip_1] = hostnames_1;

  // Build data for the second NetBiosClient which also recieves the response
  // from |source_ip_2|.
  const net::IPEndPoint source_ip_2(net::IPAddress(2, 4, 8, 16), 137);
  const Hostnames hostnames_2 = {duplicate_hostname};

  std::map<net::IPEndPoint, Hostnames> netbios_client_2;
  netbios_client_2[source_ip_2] = hostnames_2;

  // Build the map of expected results.
  HostMap expected_results;
  expected_results[duplicate_hostname] = source_ip_2.address();

  // Add the entry for a NetBios Clients that return packets.
  AddNetBiosClient(netbios_client_1);
  AddNetBiosClient(netbios_client_2);

  host_locator_ = std::make_unique<NetBiosHostLocator>(
      get_interfaces_, client_factory_, fake_provider_client_.get(),
      std::move(timer_));

  host_locator_->FindHosts(
      base::BindOnce(&ExpectResultsEqual, expected_results));
  task_runner_->FastForwardBy(base::Seconds(kNetBiosDiscoveryTimeoutSeconds));
}

}  // namespace ash::smb_client
