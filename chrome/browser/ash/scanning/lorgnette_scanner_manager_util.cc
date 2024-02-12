// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_util.h"

#include "base/strings/string_util.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash {

namespace {

// Regular expressions used to determine whether a device name contains an IPv4
// address or URL.
constexpr char kIpv4Pattern[] = R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))";
constexpr char kUrlPattern[] = R"((://))";

// Prefixes of backends that need to be checked for duplicate zeroconf and
// lorgnette records.
constexpr char kEpsondsNetworkPrefix[] = "epsonds:net:";
constexpr char kEpson2NetworkPrefix[] = "epson2:net:";

// Names of scanner protocol types.
constexpr char kMopriaProtocolType[] = "Mopria";
constexpr char kWsdProtocolType[] = "WSD";

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

bool MergeDuplicateScannerRecords(lorgnette::ScannerInfo* scanner_out,
                                  Scanner& zeroconf_scanner) {
  // Currently only epson2 and epsonds are detected by both lorgnette and
  // zeroconf.
  if (!scanner_out->name().starts_with(kEpson2NetworkPrefix) &&
      !scanner_out->name().starts_with(kEpsondsNetworkPrefix)) {
    return false;
  }

  auto device_names =
      zeroconf_scanner.device_names.find(ScanProtocol::kLegacyNetwork);
  if (device_names == zeroconf_scanner.device_names.end()) {
    return false;
  }
  for (ScannerDeviceName& device_name : device_names->second) {
    std::string alt_name = device_name.device_name;
    if (device_name.device_name.starts_with(kEpsondsNetworkPrefix)) {
      alt_name.replace(0, strlen(kEpsondsNetworkPrefix), kEpson2NetworkPrefix);
    } else if (device_name.device_name.starts_with(kEpson2NetworkPrefix)) {
      alt_name.replace(0, strlen(kEpson2NetworkPrefix), kEpsondsNetworkPrefix);
    }

    // epson2 and epsonds scanners can be detected through both lorgnette and
    // zeroconf.  When we get two records for the same scanner, we want to
    // prefer the lorgnette connection info because it is already validated,
    // but we want to prefer the zeroconf display strings because they match
    // the information shown in the printer settings page.  To get the desired
    // result, copy the desired fields from the zeroconf record into the
    // lorgnette record and then disable further use of the zeroconf entry.
    if (scanner_out->name() == device_name.device_name ||
        scanner_out->name() == alt_name) {
      scanner_out->set_manufacturer(zeroconf_scanner.manufacturer);
      scanner_out->set_model(zeroconf_scanner.model);
      scanner_out->set_display_name(zeroconf_scanner.display_name);
      if (!zeroconf_scanner.uuid.empty()) {
        scanner_out->set_device_uuid(zeroconf_scanner.uuid);
      }
      device_name.usable = false;
      return true;
    }
  }
  return false;
}

std::string ProtocolTypeForScanner(const lorgnette::ScannerInfo& scanner) {
  // sane-airscan implements two protocols.  For other backends, just return
  // the backend name.
  if (scanner.name().starts_with("airscan:escl:")) {
    return kMopriaProtocolType;
  } else if (scanner.name().starts_with("airscan:wsd:")) {
    return kWsdProtocolType;
  } else if (scanner.name().starts_with("ippusb:escl:")) {
    return kMopriaProtocolType;
  } else {
    return base::ToLowerASCII(
        scanner.name().substr(0, scanner.name().find(':')));
  }
}

}  // namespace ash
