// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility functions for the LorgnetteScannerManager.

#ifndef CHROME_BROWSER_ASH_SCANNING_LORGNETTE_SCANNER_MANAGER_UTIL_H_
#define CHROME_BROWSER_ASH_SCANNING_LORGNETTE_SCANNER_MANAGER_UTIL_H_

#include <string>

#include "chromeos/ash/components/scanning/scanner.h"

namespace lorgnette {

class ScannerInfo;

}

namespace ash {

// Attempts to parse `scanner_name` to find an IP address and determine the scan
// protocol it corresponds to. If an IP address is found, it is returned in
// `ip_address_out`.
void ParseScannerName(const std::string& scanner_name,
                      std::string& ip_address_out,
                      ScanProtocol& protocol_out);

// Attempts to find a legacy network device name in `zeroconf_scanner` that
// matches the connection string in `scanner_out`.  If a match is found, updates
// `scanner_out` with the display information from `zeroconf_scanner` and
// marks the matched device name in `zeroconf_scanner` as unusable.  Returns
// true if any merging was done.
bool MergeDuplicateScannerRecords(lorgnette::ScannerInfo* scanner_out,
                                  Scanner& zeroconf_scanner);

// Converts the SANE connection info to a "protocol type", such as Mopria, WSD,
// or epson2.
std::string ProtocolTypeForScanner(const lorgnette::ScannerInfo& scanner);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_LORGNETTE_SCANNER_MANAGER_UTIL_H_
