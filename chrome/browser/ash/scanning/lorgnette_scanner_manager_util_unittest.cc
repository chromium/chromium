// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_util.h"

#include <string>

#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
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

TEST(LorgnetteScannerManagerUtilTest, MergeNonQualifiedRecordsFails) {
  lorgnette::ScannerInfo scanner;
  scanner.set_name("ippusb:escl:therest");
  Scanner zeroconf_scanner;
  EXPECT_FALSE(MergeDuplicateScannerRecords(&scanner, zeroconf_scanner));
}

TEST(LorgnetteScannerManagerUtilTest, MergeNoLegacyNetworkFails) {
  lorgnette::ScannerInfo scanner;
  scanner.set_name("epson2:net:10.0.0.0");
  Scanner zeroconf_scanner;
  EXPECT_FALSE(MergeDuplicateScannerRecords(&scanner, zeroconf_scanner));

  scanner.set_name("epsonds:net:10.0.0.0");
  EXPECT_FALSE(MergeDuplicateScannerRecords(&scanner, zeroconf_scanner));
}

TEST(LorgnetteScannerManagerUtilTest, MergeNoDuplicatesFails) {
  lorgnette::ScannerInfo scanner;
  scanner.set_name("epson2:net:10.0.0.0");
  Scanner zeroconf_scanner;
  zeroconf_scanner.display_name = "Scanner";
  zeroconf_scanner.manufacturer = "GoogleTest";
  zeroconf_scanner.model = "model";
  zeroconf_scanner.device_names[ScanProtocol::kLegacyNetwork].emplace(
      "pixma:MF1234_10.0.0.0", true);
  EXPECT_FALSE(MergeDuplicateScannerRecords(&scanner, zeroconf_scanner));
  EXPECT_TRUE(zeroconf_scanner.device_names[ScanProtocol::kLegacyNetwork]
                  .begin()
                  ->usable);
  EXPECT_EQ(scanner.display_name(), "");
  EXPECT_EQ(scanner.manufacturer(), "");
  EXPECT_EQ(scanner.model(), "");

  scanner.set_name("epsonds:net:10.0.0.0");
  EXPECT_FALSE(MergeDuplicateScannerRecords(&scanner, zeroconf_scanner));
  EXPECT_TRUE(zeroconf_scanner.device_names[ScanProtocol::kLegacyNetwork]
                  .begin()
                  ->usable);
  EXPECT_EQ(scanner.display_name(), "");
  EXPECT_EQ(scanner.manufacturer(), "");
  EXPECT_EQ(scanner.model(), "");
}

TEST(LorgnetteScannerManagerUtilTest, MergeEpson2DuplicateWithEpson2Succeeds) {
  lorgnette::ScannerInfo scanner;
  scanner.set_name("epson2:net:10.0.0.0");
  scanner.set_device_uuid("9876-5432");
  Scanner zeroconf_scanner;
  zeroconf_scanner.display_name = "Scanner";
  zeroconf_scanner.manufacturer = "GoogleTest";
  zeroconf_scanner.model = "model";
  zeroconf_scanner.device_names[ScanProtocol::kLegacyNetwork].emplace(
      "epson2:net:10.0.0.0", true);
  zeroconf_scanner.uuid = "1234-5678";
  EXPECT_TRUE(MergeDuplicateScannerRecords(&scanner, zeroconf_scanner));
  EXPECT_FALSE(zeroconf_scanner.device_names[ScanProtocol::kLegacyNetwork]
                   .begin()
                   ->usable);
  EXPECT_EQ(scanner.display_name(), "Scanner");
  EXPECT_EQ(scanner.manufacturer(), "GoogleTest");
  EXPECT_EQ(scanner.model(), "model");
  EXPECT_EQ(scanner.device_uuid(), "1234-5678");
}

TEST(LorgnetteScannerManagerUtilTest, MergeEpson2DuplicateWithEpsondsSucceeds) {
  lorgnette::ScannerInfo scanner;
  scanner.set_name("epson2:net:10.0.0.0");
  scanner.set_device_uuid("9876-5432");
  Scanner zeroconf_scanner;
  zeroconf_scanner.display_name = "Scanner";
  zeroconf_scanner.manufacturer = "GoogleTest";
  zeroconf_scanner.model = "model";
  zeroconf_scanner.device_names[ScanProtocol::kLegacyNetwork].emplace(
      "epsonds:net:10.0.0.0", true);
  EXPECT_TRUE(MergeDuplicateScannerRecords(&scanner, zeroconf_scanner));
  EXPECT_FALSE(zeroconf_scanner.device_names[ScanProtocol::kLegacyNetwork]
                   .begin()
                   ->usable);
  EXPECT_EQ(scanner.display_name(), "Scanner");
  EXPECT_EQ(scanner.manufacturer(), "GoogleTest");
  EXPECT_EQ(scanner.model(), "model");
  EXPECT_EQ(scanner.device_uuid(), "9876-5432");
}

TEST(LorgnetteScannerManagerUtilTest, MergeEpsondsDuplicateWithEpson2Succeeds) {
  lorgnette::ScannerInfo scanner;
  scanner.set_name("epsonds:net:10.0.0.0");
  scanner.set_device_uuid("9876-5432");
  Scanner zeroconf_scanner;
  zeroconf_scanner.display_name = "Scanner";
  zeroconf_scanner.manufacturer = "GoogleTest";
  zeroconf_scanner.model = "model";
  zeroconf_scanner.device_names[ScanProtocol::kLegacyNetwork].emplace(
      "epson2:net:10.0.0.0", true);
  zeroconf_scanner.uuid = "1234-5678";
  EXPECT_TRUE(MergeDuplicateScannerRecords(&scanner, zeroconf_scanner));
  EXPECT_FALSE(zeroconf_scanner.device_names[ScanProtocol::kLegacyNetwork]
                   .begin()
                   ->usable);
  EXPECT_EQ(scanner.display_name(), "Scanner");
  EXPECT_EQ(scanner.manufacturer(), "GoogleTest");
  EXPECT_EQ(scanner.model(), "model");
  EXPECT_EQ(scanner.device_uuid(), "1234-5678");
}

TEST(LorgnetteScannerManagerUtilTest,
     MergeEpsondsDuplicateWithEpsondsSucceeds) {
  lorgnette::ScannerInfo scanner;
  scanner.set_name("epsonds:net:10.0.0.0");
  scanner.set_device_uuid("9876-5432");
  Scanner zeroconf_scanner;
  zeroconf_scanner.display_name = "Scanner";
  zeroconf_scanner.manufacturer = "GoogleTest";
  zeroconf_scanner.model = "model";
  zeroconf_scanner.device_names[ScanProtocol::kLegacyNetwork].emplace(
      "epsonds:net:10.0.0.0", true);
  EXPECT_TRUE(MergeDuplicateScannerRecords(&scanner, zeroconf_scanner));
  EXPECT_FALSE(zeroconf_scanner.device_names[ScanProtocol::kLegacyNetwork]
                   .begin()
                   ->usable);
  EXPECT_EQ(scanner.display_name(), "Scanner");
  EXPECT_EQ(scanner.manufacturer(), "GoogleTest");
  EXPECT_EQ(scanner.model(), "model");
  EXPECT_EQ(scanner.device_uuid(), "9876-5432");
}

TEST(LorgnetteScannerManagerUtilTest, ProtocolTypeMopriaNet) {
  lorgnette::ScannerInfo info;
  info.set_name("airscan:escl:therest");
  EXPECT_EQ(ProtocolTypeForScanner(info), "Mopria");
}

TEST(LorgnetteScannerManagerUtilTest, ProtocolTypeMopriaUSB) {
  lorgnette::ScannerInfo info;
  info.set_name("ippusb:escl:therest");
  EXPECT_EQ(ProtocolTypeForScanner(info), "Mopria");
}

TEST(LorgnetteScannerManagerUtilTest, ProtocolTypeWSD) {
  lorgnette::ScannerInfo info;
  info.set_name("airscan:wsd:therest");
  EXPECT_EQ(ProtocolTypeForScanner(info), "WSD");
}

TEST(LorgnetteScannerManagerUtilTest, ProtocolTypeOther) {
  lorgnette::ScannerInfo info;
  info.set_name("epson2:net:therest");
  EXPECT_EQ(ProtocolTypeForScanner(info), "epson2");

  info.set_name("epsonds:libusb:001:002");
  EXPECT_EQ(ProtocolTypeForScanner(info), "epsonds");

  info.set_name("pixma:04a91234_ABC321");
  EXPECT_EQ(ProtocolTypeForScanner(info), "pixma");

  info.set_name("fujitsu:fi-8120:10000");
  EXPECT_EQ(ProtocolTypeForScanner(info), "fujitsu");
}

}  // namespace ash
