// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/google_services_connectivity_routine_util.h"

#include <array>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
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

// Deduped hostnames across every service the routine probes. Sourced
// from Chrome Enterprise support articles 14113552 and 6334001, and
// the Chrome Remote Desktop network guide. Wildcard entries
// (`*.gvt1.com`, `*.android.com`, `*.googleapis.com`, etc.) are
// intentionally excluded in favor of specific instances that can
// actually be resolved. The XMPP GCM/FCM endpoints
// (`gcm-xmpp.googleapis.com`, `fcm-xmpp.googleapis.com`) are also
// excluded: they serve XMPP over TLS on :443 and reply HTTP/0.9, which
// shill's HTTPS probe reads as a failure. Firewall allowlists still
// need them; connectivity diagnostics cannot. The Drive API is reached
// via `www.googleapis.com`, already listed below.
constexpr auto kGoogleServicesHostnames = std::to_array<std::string_view>({
    // Zero-touch enrollment, admin-driven enrollment, and policy fetches.
    "m.google.com",
    "chromeos-ca.gstatic.com",
    "clients3.google.com",
    "www.googleapis.com",
    // Login / sign-in screen.
    "accounts.google.com",
    "www.google.com",
    "www.gstatic.com",
    "ssl.gstatic.com",
    "oauthaccountmanager.googleapis.com",
    // ChromeOS auto-update.
    "tools.google.com",
    "dl.google.com",
    "dl-ssl.google.com",
    "edgedl.me.gvt1.com",
    "cros-omahaproxy.appspot.com",
    "omahaproxy.appspot.com",
    // Chrome Remote Desktop.
    "remotedesktop.google.com",
    "remotedesktop-pa.googleapis.com",
    "instantmessaging-pa.googleapis.com",
    // Chrome Web Store / policy-installed extensions.
    "chrome.google.com",
    "clients2.google.com",
    "clients2.googleusercontent.com",
    "update.googleapis.com",
    "lh3.ggpht.com",
    "lh4.ggpht.com",
    "lh5.ggpht.com",
    "lh6.ggpht.com",
    // Android Play Store on ChromeOS.
    "play.google.com",
    "android.googleapis.com",
    "android.apis.google.com",
    "android.clients.google.com",
    "gcm-http.googleapis.com",
    "fcm.googleapis.com",
    "gmscompliance-pa.googleapis.com",
    "connectivitycheck.android.com",
    "connectivitycheck.gstatic.com",
    "clients5.google.com",
    "clients6.google.com",
    "pki.google.com",
    // Google Drive / DriveFS.
    "drive.google.com",
    "docs.google.com",
    // Automatic time zone resolution via Google Maps Timezone API.
    // See chromeos/ash/components/timezone/timezone_request.cc.
    "maps.googleapis.com",
});

}  // namespace

base::span<const std::string_view> GetGoogleServicesHostnames() {
  return kGoogleServicesHostnames;
}

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
