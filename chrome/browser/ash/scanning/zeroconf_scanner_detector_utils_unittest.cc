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

// POD struct for CreateSaneScannerServiceTypeTest.
struct CreateSaneScannerServiceTypeTestParams {
  std::string service_type;
  ScanProtocol protocol;
  std::string device_name;
};

// Constructs a net::IPAddress from `str`. The input must be a valid IP format.
net::IPAddress IpAddressFromString(const std::string& str) {
  net::IPAddress ip_addr;
  CHECK(ip_addr.AssignFromIPLiteral(str));
  return ip_addr;
}

}  // namespace

class CreateSaneScannerServiceTypeTest
    : public testing::Test,
      public testing::WithParamInterface<
          CreateSaneScannerServiceTypeTestParams> {
 public:
  CreateSaneScannerServiceTypeTest() = default;
  CreateSaneScannerServiceTypeTest(const CreateSaneScannerServiceTypeTest&) =
      delete;
  CreateSaneScannerServiceTypeTest& operator=(
      const CreateSaneScannerServiceTypeTest&) = delete;

  const CreateSaneScannerServiceTypeTestParams params() const {
    return GetParam();
  }
};

// Test that a scanner can be created for each supported service type.
TEST_P(CreateSaneScannerServiceTypeTest, SupportedServiceType) {
  net::IPAddress ip_address = IpAddressFromString("101.102.103.104");

  absl::optional<Scanner> maybe_scanner = CreateSaneScanner(
      "EPSON scanner", params().service_type, "eSCL", ip_address, 8080);

  ASSERT_TRUE(maybe_scanner.has_value());
  auto scanner = maybe_scanner.value();

  EXPECT_EQ(scanner.display_name, "EPSON scanner");

  auto device_names = scanner.device_names[params().protocol];
  ASSERT_TRUE(device_names.size() > 0);
  EXPECT_EQ(device_names.begin()->device_name, params().device_name);

  ASSERT_TRUE(scanner.ip_addresses.size() > 0);
  EXPECT_EQ(*scanner.ip_addresses.begin(), ip_address);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CreateSaneScannerServiceTypeTest,
    testing::Values(
        CreateSaneScannerServiceTypeTestParams{
            /*service_type=*/ZeroconfScannerDetector::kEsclServiceType,
            /*protocol=*/ScanProtocol::kEscl,
            /*device_name=*/
            "airscan:escl:EPSON scanner:http://101.102.103.104:8080/eSCL/"},
        CreateSaneScannerServiceTypeTestParams{
            /*service_type=*/ZeroconfScannerDetector::kEsclsServiceType,
            /*protocol=*/ScanProtocol::kEscls,
            /*device_name=*/
            "airscan:escl:EPSON scanner:https://101.102.103.104:8080/eSCL/"},
        CreateSaneScannerServiceTypeTestParams{
            /*service_type=*/ZeroconfScannerDetector::
                kGenericScannerServiceType,
            /*protocol=*/ScanProtocol::kLegacyNetwork,
            /*device_name=*/
            "epsonds:net:101.102.103.104"}));

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

// Test that CreateSaneScanner handles scanners which don't report an rs value.
TEST(CreateSaneScanner, NoRsValue) {
  absl::optional<Scanner> maybe_scanner = CreateSaneScanner(
      "name", ZeroconfScannerDetector::kEsclServiceType, absl::nullopt,
      IpAddressFromString("101.102.103.104"), 8080);

  ASSERT_TRUE(maybe_scanner.has_value());

  auto device_names = maybe_scanner.value().device_names[ScanProtocol::kEscl];
  ASSERT_TRUE(device_names.size() > 0);
  EXPECT_EQ(device_names.begin()->device_name,
            "airscan:escl:name:http://101.102.103.104:8080/eSCL/");
}

// Test that CreateSaneScanner fails when an invalid IP address is passed in.
TEST(CreateSaneScanner, InvalidIpAddress) {
  absl::optional<Scanner> maybe_scanner =
      CreateSaneScanner("name", ZeroconfScannerDetector::kEsclServiceType,
                        "eSCL", net::IPAddress(), 8080);

  EXPECT_FALSE(maybe_scanner.has_value());
}

// Test that CreateSaneScanner fails for a generic, non-Epson scanner.
TEST(CreateSaneScanner, GenericNonEpsonScanner) {
  absl::optional<Scanner> maybe_scanner = CreateSaneScanner(
      "name", ZeroconfScannerDetector::kGenericScannerServiceType,
      absl::nullopt, IpAddressFromString("101.102.103.104"), 8080);

  EXPECT_FALSE(maybe_scanner.has_value());
}

}  // namespace ash
