// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_ZEROCONF_SCANNER_DETECTOR_UTILS_H_
#define CHROME_BROWSER_ASH_SCANNING_ZEROCONF_SCANNER_DETECTOR_UTILS_H_

#include <optional>
#include <string>

#include "chromeos/ash/components/scanning/scanner.h"
#include "net/base/ip_address.h"

namespace ash {

// Creates a Scanner with a device name that can be used to interact with a
// scanner via the given backend. If errors occur, std::nullopts is
// returned. The device name format depends on the backend. Sane-airscan
// scanners will have an "airscan:escl:name:url" string where name is an
// arbitrary name, while Epsonds scanners will have "epsonds:net:|IP|". The IP
// address is used instead of the host name since the backend may not be able to
// resolve host names it did not discover itself. See mdns_make_escl_endpoint()
// at https://github.com/alexpevzner/sane-airscan/blob/master/airscan-mdns.c for
// more details.
std::optional<Scanner> CreateSaneScanner(const std::string& name,
                                         const std::string& service_type,
                                         const std::string& manufacturer,
                                         const std::string& model,
                                         const std::string& uuid,
                                         const std::optional<std::string>& rs,
                                         const std::vector<std::string>& pdl,
                                         const net::IPAddress& ip_address,
                                         int port,
                                         bool usable = true);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_ZEROCONF_SCANNER_DETECTOR_UTILS_H_
