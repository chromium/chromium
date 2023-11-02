// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_util.h"

#include "third_party/re2/src/re2/re2.h"

namespace ash {

namespace {

// Regular expressions used to determine whether a device name contains an IPv4
// address or URL.
constexpr char kIpv4Pattern[] = R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))";
constexpr char kUrlPattern[] = R"((://))";

}  // namespace

void ParseScannerName(const std::string& scanner_name,
                      std::string& ip_address_out,
                      ScanProtocol& protocol_out) {
  if (RE2::PartialMatch(scanner_name, kIpv4Pattern, &ip_address_out) ||
      RE2::PartialMatch(scanner_name, kUrlPattern)) {
    protocol_out = ScanProtocol::kLegacyNetwork;
    return;
  }

  protocol_out = ScanProtocol::kLegacyUsb;
}

}  // namespace ash
