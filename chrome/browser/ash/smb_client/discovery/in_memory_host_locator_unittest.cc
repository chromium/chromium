// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/discovery/in_memory_host_locator.h"

#include <map>
#include <string>

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::smb_client {

namespace {

// Expects |actual_hosts| to not equal |hosts|.
void ExpectMapEntriesNotEqual(const HostMap& hosts,
                              bool success,
                              const HostMap& actual_hosts) {
  EXPECT_TRUE(success);
  EXPECT_NE(hosts, actual_hosts);
}

// Expects |actual_hosts| to equal |expected_hosts|.
void ExpectMapEntriesEqual(const HostMap& expected_hosts,
                           bool success,
                           const HostMap& actual_hosts) {
  EXPECT_TRUE(success);
  EXPECT_EQ(expected_hosts, actual_hosts);
}

}  // namespace

class InMemoryHostLocatorTest : public testing::Test {
 public:
  InMemoryHostLocatorTest() = default;

  InMemoryHostLocatorTest(const InMemoryHostLocatorTest&) = delete;
  InMemoryHostLocatorTest& operator=(const InMemoryHostLocatorTest&) = delete;

  ~InMemoryHostLocatorTest() override = default;

 protected:
  void ExpectHostMapEqual(const HostMap& hosts) {
    locator_.FindHosts(base::BindOnce(&ExpectMapEntriesEqual, hosts));
  }

  void ExpectHostMapNotEqual(const HostMap& hosts) {
    locator_.FindHosts(base::BindOnce(&ExpectMapEntriesNotEqual, hosts));
  }

  InMemoryHostLocator locator_;
};

TEST_F(InMemoryHostLocatorTest, AddHostShouldNotBeEqual) {
  HostMap incorrect_map;
  incorrect_map["host1"] = {1, 2, 3, 4};

  // Add a different host entry using AddHost().
  locator_.AddHost("host2", {5, 6, 7, 8});

  ExpectHostMapNotEqual(incorrect_map);
}

TEST_F(InMemoryHostLocatorTest, AddHostsShouldNotBeEqual) {
  HostMap incorrect_map;
  incorrect_map["host2"] = {6, 7, 8, 9};

  // Add a different host entry using AddHosts().
  HostMap host_map;
  host_map["host1"] = {1, 2, 3, 4};
  locator_.AddHosts(host_map);

  ExpectHostMapNotEqual(incorrect_map);
}

TEST_F(InMemoryHostLocatorTest, ShouldFindNoHosts) {
  ExpectHostMapEqual(HostMap());
}

TEST_F(InMemoryHostLocatorTest, ShouldFindOneHost) {
  locator_.AddHost("host1", {1, 2, 3, 4});

  HostMap expected;
  expected["host1"] = {1, 2, 3, 4};
  ExpectHostMapEqual(expected);
}

TEST_F(InMemoryHostLocatorTest, ShouldFindMultipleHosts) {
  HostMap host_map;
  host_map["host1"] = {1, 2, 3, 4};
  host_map["host2"] = {3, 4, 5, 6};
  locator_.AddHosts(host_map);

  ExpectHostMapEqual(host_map);
}

TEST_F(InMemoryHostLocatorTest, ShouldOverwriteHostWithSameName) {
  locator_.AddHost("host1", {1, 2, 3, 4});
  locator_.AddHost("host1", {5, 6, 7, 8});

  HostMap expected;
  expected["host1"] = {5, 6, 7, 8};
  ExpectHostMapEqual(expected);
}

TEST_F(InMemoryHostLocatorTest, ShouldRemoveHost) {
  HostMap host_map;
  host_map["host1"] = {1, 2, 3, 4};
  host_map["host2"] = {3, 4, 5, 6};
  locator_.AddHosts(host_map);

  ExpectHostMapEqual(host_map);

  // Remove a host.
  locator_.RemoveHost("host2");
  ExpectHostMapNotEqual(host_map);

  // The locator should only return the host that was not removed.
  HostMap expected;
  expected["host1"] = {1, 2, 3, 4};
  ExpectHostMapEqual(expected);
}

TEST_F(InMemoryHostLocatorTest, AddHostsShouldKeepPreviousHosts) {
  locator_.AddHost("host1", {1, 2, 3, 4});

  HostMap host_map;
  host_map["host2"] = {5, 6, 7, 8};
  locator_.AddHosts(host_map);

  HostMap expected;
  expected["host1"] = {1, 2, 3, 4};
  expected["host2"] = {5, 6, 7, 8};
  ExpectHostMapEqual(expected);
}

TEST_F(InMemoryHostLocatorTest, AddHostsShouldKeepPreviousHostsAndOverwrite) {
  locator_.AddHost("host1", {1, 2, 3, 4});
  locator_.AddHost("host2", {5, 6, 7, 8});

  // Add a host with same hostname but different address, along with a new host.
  HostMap host_map;
  host_map["host2"] = {15, 16, 17, 18};
  host_map["host3"] = {25, 26, 27, 28};
  locator_.AddHosts(host_map);

  // The host with the same name should be overwritten, and the new host
  // should be added.
  HostMap expected;
  expected["host1"] = {1, 2, 3, 4};
  expected["host2"] = {15, 16, 17, 18};
  expected["host3"] = {25, 26, 27, 28};
  ExpectHostMapEqual(expected);
}

}  // namespace ash::smb_client
