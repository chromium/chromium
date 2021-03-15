// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/zeroconf_scanner_detector_utils.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/scanning/zeroconf_scanner_detector.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Sets the values of |scheme| and |protocol| based on |service_type|.
void SetSchemeAndProtocol(const std::string& service_type,
                          std::string& scheme_out,
                          chromeos::ScanProtocol& protocol_out) {
  if (service_type == ZeroconfScannerDetector::kEsclsServiceType) {
    scheme_out = "https";
    protocol_out = chromeos::ScanProtocol::kEscls;
  } else if (service_type == ZeroconfScannerDetector::kEsclServiceType) {
    scheme_out = "http";
    protocol_out = chromeos::ScanProtocol::kEscl;
  } else {
    NOTREACHED() << "Zeroconf scanner with unknown service type: "
                 << service_type;
  }
}

// Creates a device name compatible with the sane-airscan backend. Returns the
// name on success and an empty string on failure.
std::string CreateDeviceName(const std::string& name,
                             const std::string& scheme,
                             const std::string& rs,
                             const net::IPAddress& ip_address,
                             int port) {
  std::string path;
  if (rs == "none")
    path = "eSCL/";
  else if (!rs.empty())
    path = rs + "/";

  // Replace colons in the instance name to prevent them from breaking
  // sane-airscan's device name parsing logic.
  std::string sanitized_name;
  base::ReplaceChars(name, ":", "-", &sanitized_name);

  const std::string ip_address_str =
      ip_address.IsIPv6()
          ? base::StringPrintf("[%s]", ip_address.ToString().c_str())
          : ip_address.ToString();
  GURL url(base::StringPrintf("%s://%s:%d/%s", scheme.c_str(),
                              ip_address_str.c_str(), port, path.c_str()));
  if (!url.is_valid()) {
    LOG(ERROR) << "Cannot create device name with invalid URL: "
               << url.possibly_invalid_spec();
    return "";
  }

  return base::StringPrintf("airscan:escl:%s:%s", sanitized_name.c_str(),
                            url.spec().c_str());
}

}  // namespace

base::Optional<chromeos::Scanner> CreateSaneAirscanScanner(
    const std::string& name,
    const std::string& service_type,
    const std::string& rs,
    const net::IPAddress& ip_address,
    int port,
    bool usable) {
  std::string scheme;
  chromeos::ScanProtocol protocol = chromeos::ScanProtocol::kUnknown;
  SetSchemeAndProtocol(service_type, scheme, protocol);
  const std::string device_name =
      CreateDeviceName(name, scheme, rs, ip_address, port);
  if (device_name.empty())
    return base::nullopt;

  chromeos::Scanner scanner;
  scanner.display_name = name;
  scanner.device_names[protocol].emplace(
      chromeos::ScannerDeviceName(device_name, usable));
  scanner.ip_addresses.insert(ip_address);
  return scanner;
}

}  // namespace ash
