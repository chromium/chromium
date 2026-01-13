// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/google_services_connectivity_routine_util.h"

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"

namespace ash::network_diagnostics {

namespace {

using ConnectivityResultCode =
    hosts_connectivity_diagnostics::ConnectivityResultCode;
using ProblemType =
    chromeos::network_diagnostics::mojom::GoogleServicesConnectivityProblemType;
using ProxyProblemType = chromeos::network_diagnostics::mojom::
    GoogleServicesConnectivityProxyProblemType;

constexpr auto kConnectionProblemTypeMap =
    base::MakeFixedFlatMap<ConnectivityResultCode, ProblemType>({
        {ConnectivityResultCode::INTERNAL_ERROR, ProblemType::kInternalError},
        {ConnectivityResultCode::NO_VALID_HOSTNAME,
         ProblemType::kInternalError},
        {ConnectivityResultCode::UNKNOWN_ERROR, ProblemType::kUnknownError},
        {ConnectivityResultCode::CONNECTION_FAILURE,
         ProblemType::kConnectionFailure},
        {ConnectivityResultCode::CONNECTION_TIMEOUT,
         ProblemType::kConnectionTimeout},
        {ConnectivityResultCode::DNS_RESOLUTION_ERROR,
         ProblemType::kDnsResolutionError},
        {ConnectivityResultCode::SSL_CONNECTION_ERROR,
         ProblemType::kSSLConnectionError},
        {ConnectivityResultCode::PEER_CERTIFICATE_ERROR,
         ProblemType::kPeerCertificateError},
        {ConnectivityResultCode::HTTP_ERROR, ProblemType::kHttpError},
        {ConnectivityResultCode::NO_NETWORK_ERROR,
         ProblemType::kNoNetworkError},
    });

constexpr auto kProxyProblemTypeMap =
    base::MakeFixedFlatMap<ConnectivityResultCode, ProxyProblemType>({
        {ConnectivityResultCode::PROXY_DNS_RESOLUTION_ERROR,
         ProxyProblemType::kProxyDnsResolutionError},
        {ConnectivityResultCode::PROXY_CONNECTION_FAILURE,
         ProxyProblemType::kProxyConnectionFailure},
    });

}  // namespace

std::optional<std::string> ToOptionalStrIfNonEmpty(const std::string& value) {
  return value.empty() ? std::nullopt : std::make_optional(value);
}

bool IsSuccessfulOrUnspecifiedResult(
    hosts_connectivity_diagnostics::ConnectivityResultCode result_code) {
  return result_code == ConnectivityResultCode::SUCCESS ||
         result_code == ConnectivityResultCode::UNSPECIFIED;
}

bool IsProxyConnectionError(
    hosts_connectivity_diagnostics::ConnectivityResultCode result_code) {
  return result_code == ConnectivityResultCode::PROXY_DNS_RESOLUTION_ERROR ||
         result_code == ConnectivityResultCode::PROXY_CONNECTION_FAILURE;
}

bool IsNoValidProxyError(
    hosts_connectivity_diagnostics::ConnectivityResultCode result_code) {
  return result_code == ConnectivityResultCode::NO_VALID_PROXY;
}

chromeos::network_diagnostics::mojom::GoogleServicesConnectivityProblemType
ToConnectivityProblemType(
    hosts_connectivity_diagnostics::ConnectivityResultCode result_code) {
  auto it = kConnectionProblemTypeMap.find(result_code);
  if (it != kConnectionProblemTypeMap.end()) {
    return it->second;
  }
  DLOG(WARNING) << "Unexpected result code for connection error: "
                << static_cast<int>(result_code);
  return ProblemType::kInternalError;
}

chromeos::network_diagnostics::mojom::GoogleServicesConnectivityProxyProblemType
ToProxyProblemType(
    hosts_connectivity_diagnostics::ConnectivityResultCode result_code) {
  auto it = kProxyProblemTypeMap.find(result_code);
  if (it != kProxyProblemTypeMap.end()) {
    return it->second;
  }
  DLOG(WARNING) << "Unexpected result code for proxy error: "
                << static_cast<int>(result_code);
  return ProxyProblemType::kProxyConnectionFailure;
}

}  // namespace ash::network_diagnostics
