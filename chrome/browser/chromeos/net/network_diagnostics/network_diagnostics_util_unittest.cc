// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_util.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace network_diagnostics {

TEST(NetworkDiagnosticsUtilTest, TestGetRandomString) {
  int length = 8;
  auto random_string = util::GetRandomString(length);
  // Ensure that the length equals |length| and all characters are in between
  // 'a'-'z', inclusive.
  ASSERT_EQ(length, random_string.size());
  for (char const& c : random_string) {
    ASSERT_TRUE(c >= 'a' && c <= 'z');
  }
}

TEST(NetworkDiagnosticsUtilTest, TestGetRandomHosts) {
  int num_hosts = 10;
  int prefix_length = 8;
  std::vector<std::string> random_hosts =
      util::GetRandomHosts(num_hosts, prefix_length);
  // Ensure |random_hosts| has unique entries.
  std::sort(random_hosts.begin(), random_hosts.end());
  ASSERT_TRUE(std::adjacent_find(random_hosts.begin(), random_hosts.end()) ==
              random_hosts.end());
}

}  // namespace network_diagnostics
}  // namespace chromeos
