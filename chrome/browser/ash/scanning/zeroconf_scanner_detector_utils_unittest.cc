// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/zeroconf_scanner_detector_utils.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/ash/scanning/zeroconf_scanner_detector.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Constructs a net::IPAddress from `str`. The input must be a valid IP format.
net::IPAddress IpAddressFromString(const std::string& str) {
  net::IPAddress ip_addr;
  CHECK(ip_addr.AssignFromIPLiteral(str));
  return ip_addr;
}

}  // namespace

class CreateSaneScannerSlashTest
    : public testing::Test,
      public testing::WithParamInterface<std::string> {
 public:
  CreateSaneScannerSlashTest() = default;
  CreateSaneScannerSlashTest(const CreateSaneScannerSlashTest&) = delete;
  CreateSaneScannerSlashTest& operator=(const CreateSaneScannerSlashTest&) =
      delete;

  const std::string rs() const { return GetParam(); }
};

// Test that the correct scanner name is constructed for scanners which report
// rs values with slashes.
TEST_P(CreateSaneScannerSlashTest, DropsSlash) {
  absl::optional<Scanner> maybe_scanner =
      CreateSaneScanner("name", ZeroconfScannerDetector::kEsclServiceType, rs(),
                        IpAddressFromString("101.102.103.104"), 8080);

  ASSERT_TRUE(maybe_scanner.has_value());

  auto device_names = maybe_scanner.value().device_names[ScanProtocol::kEscl];

  ASSERT_TRUE(device_names.size() > 0);

  EXPECT_EQ(device_names.begin()->device_name,
            "airscan:escl:name:http://101.102.103.104:8080/eSCL/");
}

INSTANTIATE_TEST_SUITE_P(,
                         CreateSaneScannerSlashTest,
                         testing::Values("/eSCL", "eSCL/", "/eSCL/"));

}  // namespace ash
