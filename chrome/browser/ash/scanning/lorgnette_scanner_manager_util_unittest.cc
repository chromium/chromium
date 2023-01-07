// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_util.h"

#include <string>

#include "chromeos/ash/components/scanning/scanner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Test that parsing a scanner name with an IP address successfully extracts the
// IP address and sets the protocol to kLegacyNetwork.
TEST(LorgnetteScannerManagerUtilTest, ParseNameWithIPAddress) {
  const std::string scanner_name = "test:MX3100_192.168.0.3";
  std::string ip_address;
  ScanProtocol protocol;
  ParseScannerName(scanner_name, ip_address, protocol);
  EXPECT_EQ(ip_address, "192.168.0.3");
  EXPECT_EQ(protocol, ScanProtocol::kLegacyNetwork);
}

// Test that parsing a scanner name with a URL successfully sets the protocol to
// kLegacyNetwork.
TEST(LorgnetteScannerManagerUtilTest, ParseNameWithUrl) {
  const std::string scanner_name = "http://testscanner.domain.org";
  std::string ip_address;
  ScanProtocol protocol;
  ParseScannerName(scanner_name, ip_address, protocol);
  EXPECT_TRUE(ip_address.empty());
  EXPECT_EQ(protocol, ScanProtocol::kLegacyNetwork);
}

// Test that parsing a scanner name without an IP address or URL successfully
// sets the protocol to kLegacyUsb.
TEST(LorgnetteScannerManagerUtilTest, ParseNameWithVidPid) {
  const std::string scanner_name = "test:04A91752_94370B";
  std::string ip_address;
  ScanProtocol protocol;
  ParseScannerName(scanner_name, ip_address, protocol);
  EXPECT_TRUE(ip_address.empty());
  EXPECT_EQ(protocol, ScanProtocol::kLegacyUsb);
}

}  // namespace ash
