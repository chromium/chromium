// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/zeroconf_scanner_detector_utils.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/ash/scanning/zeroconf_scanner_detector.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
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

  std::optional<Scanner> maybe_scanner = CreateSaneScanner(
      "EPSON scanner", params().service_type, "EPSON", "Model", "UUID", "eSCL",
      {"pdl-1", "pdl-2"}, ip_address, 8080);

  ASSERT_TRUE(maybe_scanner.has_value());
  auto scanner = maybe_scanner.value();

  EXPECT_EQ(scanner.display_name, "EPSON scanner");
  EXPECT_EQ(scanner.manufacturer, "EPSON");
  EXPECT_EQ(scanner.model, "Model");
  EXPECT_EQ(scanner.uuid, "UUID");
  EXPECT_THAT(scanner.pdl, testing::ElementsAre("pdl-1", "pdl-2"));

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
  std::optional<Scanner> maybe_scanner =
      CreateSaneScanner("name", ZeroconfScannerDetector::kEsclServiceType,
                        "Manufacturer", "Model", /*uuid=*/"", rs(), /*pdl=*/{},
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

// Test that CreateSaneScanner handles scanners which don't report an rs value,
// manufacturer value, nor model value.
TEST(CreateSaneScanner, NoRsValue) {
  std::optional<Scanner> maybe_scanner = CreateSaneScanner(
      "name", ZeroconfScannerDetector::kEsclServiceType, /*manufacturer=*/"",
      /*model=*/"", /*uuid=*/"", /*rs=*/std::nullopt, /*pdl=*/{},
      IpAddressFromString("101.102.103.104"), 8080);

  ASSERT_TRUE(maybe_scanner.has_value());

  auto scanner = maybe_scanner.value();
  auto device_names = scanner.device_names[ScanProtocol::kEscl];
  ASSERT_TRUE(device_names.size() > 0);
  EXPECT_EQ(device_names.begin()->device_name,
            "airscan:escl:name:http://101.102.103.104:8080/eSCL/");
  EXPECT_TRUE(scanner.manufacturer.empty());
  EXPECT_TRUE(scanner.model.empty());
  EXPECT_TRUE(scanner.uuid.empty());
}

// Test that CreateSaneScanner fails when an invalid IP address is passed in.
TEST(CreateSaneScanner, InvalidIpAddress) {
  std::optional<Scanner> maybe_scanner = CreateSaneScanner(
      "name", ZeroconfScannerDetector::kEsclServiceType, "Manufacturer",
      "Model", /*uuid=*/"", "eSCL", /*pdl=*/{}, net::IPAddress(), 8080);

  EXPECT_FALSE(maybe_scanner.has_value());
}

// Test that CreateSaneScanner fails for a generic, non-Epson scanner.
TEST(CreateSaneScanner, GenericNonEpsonScanner) {
  std::optional<Scanner> maybe_scanner = CreateSaneScanner(
      "name", ZeroconfScannerDetector::kGenericScannerServiceType,
      "Manufacturer", "Model", /*uuid=*/"", std::nullopt, /*pdl=*/{},
      IpAddressFromString("101.102.103.104"), 8080);

  EXPECT_FALSE(maybe_scanner.has_value());
}

}  // namespace ash
