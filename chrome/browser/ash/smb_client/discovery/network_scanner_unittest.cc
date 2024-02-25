// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/discovery/network_scanner.h"

#include <map>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ash/smb_client/discovery/in_memory_host_locator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::smb_client {

namespace {

// Expects |actual_hosts| to equal |expected_hosts|.
void ExpectMapEntriesEqual(const HostMap& expected_hosts,
                           bool success,
                           const HostMap& actual_hosts) {
  EXPECT_TRUE(success);
  EXPECT_EQ(expected_hosts, actual_hosts);
}

void ExpectFailure(bool success, const HostMap& actual_hosts) {
  EXPECT_FALSE(success);
  EXPECT_TRUE(actual_hosts.empty());
}

}  // namespace

class NetworkScannerTest : public testing::Test {
 public:
  NetworkScannerTest() = default;

  NetworkScannerTest(const NetworkScannerTest&) = delete;
  NetworkScannerTest& operator=(const NetworkScannerTest&) = delete;

  ~NetworkScannerTest() override = default;

 protected:
  void RegisterHostLocatorWithHosts(const HostMap& hosts) {
    std::unique_ptr<InMemoryHostLocator> host_locator =
        std::make_unique<InMemoryHostLocator>();
    host_locator->AddHosts(hosts);
    scanner_.RegisterHostLocator(std::move(host_locator));
  }

  void ExpectHostMapEqual(const HostMap& expected_hosts) {
    scanner_.FindHostsInNetwork(
        base::BindOnce(&ExpectMapEntriesEqual, expected_hosts));
  }

  void ExpectCallFailure() {
    scanner_.FindHostsInNetwork(base::BindOnce(&ExpectFailure));
  }

  void ExpectResolvedHostEquals(const std::string& expected,
                                const std::string& host) {
    EXPECT_EQ(expected, scanner_.ResolveHost(host).ToString());
  }

  // Registers |hosts| with a host locator and call FindHostsInNetwork() which
  // caches the results.
  void RegisterAndCacheHosts(const HostMap& hosts) {
    RegisterHostLocatorWithHosts(hosts);
    ExpectHostMapEqual(hosts);
  }

 private:
  NetworkScanner scanner_;
};

TEST_F(NetworkScannerTest, SuccessIsFalseAndHostsMapIsEmptyWithNoLocator) {
  ExpectCallFailure();
}

TEST_F(NetworkScannerTest, ShouldFindNoHostsWithOneLocator) {
  RegisterHostLocatorWithHosts(HostMap());

  ExpectHostMapEqual(HostMap());
}

TEST_F(NetworkScannerTest, ShouldFindNoHostsWithMultipleLocators) {
  RegisterHostLocatorWithHosts(HostMap());
  RegisterHostLocatorWithHosts(HostMap());

  ExpectHostMapEqual(HostMap());
}

TEST_F(NetworkScannerTest, ShouldFindOneHostWithOneLocator) {
  HostMap hosts;
  hosts["share1"] = {1, 2, 3, 4};
  RegisterHostLocatorWithHosts(hosts);

  ExpectHostMapEqual(hosts);
}

TEST_F(NetworkScannerTest, ShouldFindMultipleHostsWithOneLocator) {
  HostMap hosts;
  hosts["share1"] = {1, 2, 3, 4};
  hosts["share2"] = {5, 6, 7, 8};
  RegisterHostLocatorWithHosts(hosts);

  ExpectHostMapEqual(hosts);
}

TEST_F(NetworkScannerTest, ShouldFindMultipleHostsWithMultipleLocators) {
  HostMap hosts1;
  hosts1["share1"] = {1, 2, 3, 4};
  hosts1["share2"] = {5, 6, 7, 8};
  RegisterHostLocatorWithHosts(hosts1);

  HostMap hosts2;
  hosts2["share3"] = {11, 12, 13, 14};
  hosts2["share4"] = {15, 16, 17, 18};
  RegisterHostLocatorWithHosts(hosts2);

  HostMap expected;
  expected["share1"] = {1, 2, 3, 4};
  expected["share2"] = {5, 6, 7, 8};
  expected["share3"] = {11, 12, 13, 14};
  expected["share4"] = {15, 16, 17, 18};

  ExpectHostMapEqual(expected);
}

TEST_F(NetworkScannerTest, ShouldResolveMultipleHostsWithSameAddress) {
  HostMap hosts1;
  hosts1["share1"] = {1, 2, 3, 4};
  hosts1["share2"] = {5, 6, 7, 8};
  RegisterHostLocatorWithHosts(hosts1);

  HostMap hosts2;
  hosts2["share2"] = {11, 12, 13, 14};
  hosts2["share3"] = {15, 16, 17, 18};
  RegisterHostLocatorWithHosts(hosts2);

  // share2 should have the value from host1 since it is found first.
  HostMap expected;
  expected["share1"] = {1, 2, 3, 4};
  expected["share2"] = {5, 6, 7, 8};
  expected["share3"] = {15, 16, 17, 18};

  ExpectHostMapEqual(expected);
}

TEST_F(NetworkScannerTest, ResolveHostReturnsEmptyStringIfNoHostFound) {
  HostMap hosts;
  // Register a hostlocator with no hosts.
  RegisterAndCacheHosts(hosts);

  // Returns an empty string since host could not be resolved.
  ExpectResolvedHostEquals("", "server");
}

TEST_F(NetworkScannerTest, ResolveHostResolvesHostsFound) {
  HostMap hosts;
  hosts["share1"] = {1, 2, 3, 4};
  hosts["share2"] = {4, 5, 6, 7};
  RegisterAndCacheHosts(hosts);

  ExpectResolvedHostEquals("1.2.3.4", "share1");
  ExpectResolvedHostEquals("4.5.6.7", "share2");

  // Returns an empty string since host could not be resolved.
  ExpectResolvedHostEquals("", "share3");
}

TEST_F(NetworkScannerTest, ResolveHostWithUppercaseHost) {
  HostMap hosts;
  hosts["share1"] = {1, 2, 3, 4};
  RegisterAndCacheHosts(hosts);

  ExpectResolvedHostEquals("1.2.3.4", "SHARE1");
}

TEST_F(NetworkScannerTest, HostsAreStoredAsLowercase) {
  HostMap hosts;
  hosts["SHARE1"] = {1, 2, 3, 4};
  hosts["sHaRe2"] = {11, 12, 13, 14};
  hosts["Share3"] = {21, 22, 23, 24};
  RegisterHostLocatorWithHosts(hosts);

  // expected_hosts should have all lowercase hosts.
  HostMap expected_hosts;
  expected_hosts["share1"] = {1, 2, 3, 4};
  expected_hosts["share2"] = {11, 12, 13, 14};
  expected_hosts["share3"] = {21, 22, 23, 24};
  ExpectHostMapEqual(expected_hosts);

  ExpectResolvedHostEquals("1.2.3.4", "share1");
  ExpectResolvedHostEquals("11.12.13.14", "share2");
  ExpectResolvedHostEquals("21.22.23.24", "share3");
}

}  // namespace ash::smb_client
