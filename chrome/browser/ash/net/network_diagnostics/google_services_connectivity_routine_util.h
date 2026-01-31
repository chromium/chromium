// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_GOOGLE_SERVICES_CONNECTIVITY_ROUTINE_UTIL_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_GOOGLE_SERVICES_CONNECTIVITY_ROUTINE_UTIL_H_

#include <optional>
#include <string>

#include "chrome/browser/ash/net/network_diagnostics/hosts_connectivity_diagnostics.pb.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"

namespace ash::network_diagnostics {

inline constexpr char kGoogleServicesConnectivityFailedToExecuteError[] =
    "Failed to execute the connectivity test.";
inline constexpr char kGoogleServicesConnectivityInvalidProtobufError[] =
    "Invalid protobuf response from connectivity test.";

// Converts a string to optional, returning nullopt for empty strings.
std::optional<std::string> ToOptionalStrIfNonEmpty(const std::string& value);

// Returns true for SUCCESS and UNSPECIFIED result codes.
bool IsSuccessfulOrUnspecifiedResult(
    hosts_connectivity_diagnostics::ConnectivityResultCode result_code);

// Returns true for proxy-specific connection errors:
// PROXY_DNS_RESOLUTION_ERROR and PROXY_CONNECTION_FAILURE.
bool IsProxyConnectionError(
    hosts_connectivity_diagnostics::ConnectivityResultCode result_code);

// Returns true for NO_VALID_PROXY result code.
bool IsNoValidProxyError(
    hosts_connectivity_diagnostics::ConnectivityResultCode result_code);

// Maps result code to GoogleServicesConnectivityProblemType for connection
// errors. Unknown/unexpected codes map to kInternalError with a warning log.
chromeos::network_diagnostics::mojom::GoogleServicesConnectivityProblemType
ToConnectivityProblemType(
    hosts_connectivity_diagnostics::ConnectivityResultCode result_code);

// Maps result code to GoogleServicesConnectivityProxyProblemType for proxy
// connection errors. Only expected for PROXY_DNS_RESOLUTION_ERROR and
// PROXY_CONNECTION_FAILURE; unexpected codes log a warning and return
// kProxyConnectionFailure as fallback.
chromeos::network_diagnostics::mojom::GoogleServicesConnectivityProxyProblemType
ToProxyProblemType(
    hosts_connectivity_diagnostics::ConnectivityResultCode result_code);

}  // namespace ash::network_diagnostics

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_GOOGLE_SERVICES_CONNECTIVITY_ROUTINE_UTIL_H_
