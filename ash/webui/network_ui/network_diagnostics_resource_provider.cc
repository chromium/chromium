// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/network_ui/network_diagnostics_resource_provider.h"

#include "ash/webui/grit/ash_webui_common_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {
namespace network_diagnostics {

namespace {

constexpr webui::LocalizedString kLocalizedStrings[] = {
    // Network Diagnostics Strings
    {"NetworkDiagnosticsLanConnectivity",
     IDS_NETWORK_DIAGNOSTICS_LAN_CONNECTIVITY},
    {"NetworkDiagnosticsSignalStrength",
     IDS_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH},
    {"NetworkDiagnosticsGatewayCanBePinged",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED},
    {"NetworkDiagnosticsHasSecureWiFiConnection",
     IDS_NETWORK_DIAGNOSTICS_HAS_SECURE_WIFI_CONNECTION},
    {"NetworkDiagnosticsDnsResolverPresent",
     IDS_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PRESENT},
    {"NetworkDiagnosticsDnsLatency", IDS_NETWORK_DIAGNOSTICS_DNS_LATENCY},
    {"NetworkDiagnosticsDnsResolution", IDS_NETWORK_DIAGNOSTICS_DNS_RESOLUTION},
    {"NetworkDiagnosticsHttpFirewall", IDS_NETWORK_DIAGNOSTICS_HTTP_FIREWALL},
    {"NetworkDiagnosticsHttpsFirewall", IDS_NETWORK_DIAGNOSTICS_HTTPS_FIREWALL},
    {"NetworkDiagnosticsHttpsLatency", IDS_NETWORK_DIAGNOSTICS_HTTPS_LATENCY},
    {"NetworkDiagnosticsCaptivePortal", IDS_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL},
    {"NetworkDiagnosticsVideoConferencing",
     IDS_NETWORK_DIAGNOSTICS_VIDEO_CONFERENCING},
    {"ArcNetworkDiagnosticsPing",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED},
    {"ArcNetworkDiagnosticsHttp", IDS_NETWORK_DIAGNOSTICS_ARC_HTTP_LATENCY},
    {"ArcNetworkDiagnosticsDnsResolution",
     IDS_NETWORK_DIAGNOSTICS_DNS_RESOLUTION},
    {"NetworkDiagnosticsConnectionGroup",
     IDS_NETWORK_DIAGNOSTICS_CONNECTION_GROUP},
    {"NetworkDiagnosticsWifiGroup", IDS_NETWORK_DIAGNOSTICS_WIFI_GROUP},
    {"NetworkDiagnosticsGatewayGroup", IDS_NETWORK_DIAGNOSTICS_GATEWAY_GROUP},
    {"NetworkDiagnosticsFirewallGroup", IDS_NETWORK_DIAGNOSTICS_FIREWALL_GROUP},
    {"NetworkDiagnosticsDnsGroup", IDS_NETWORK_DIAGNOSTICS_DNS_GROUP},
    {"NetworkDiagnosticsGoogleServicesGroup",
     IDS_NETWORK_DIAGNOSTICS_GOOGLE_SERVICES_GROUP},
    {"NetworkDiagnosticsArcGroup", IDS_NETWORK_DIAGNOSTICS_ARC_GROUP},
    {"NetworkDiagnosticsPassed", IDS_NETWORK_DIAGNOSTICS_PASSED},
    {"NetworkDiagnosticsFailed", IDS_NETWORK_DIAGNOSTICS_FAILED},
    {"NetworkDiagnosticsNotRun", IDS_NETWORK_DIAGNOSTICS_NOT_RUN},
    {"NetworkDiagnosticsRunning", IDS_NETWORK_DIAGNOSTICS_RUNNING},
    {"NetworkDiagnosticsResultPlaceholder",
     IDS_NETWORK_DIAGNOSTICS_RESULT_PLACEHOLDER},
    {"NetworkDiagnosticsRun", IDS_NETWORK_DIAGNOSTICS_RUN},
    {"SignalStrengthProblem_Weak",
     IDS_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH_PROBLEM_WEAK},
    {"GatewayPingProblem_Unreachable",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_PROBLEM_UNREACHABLE},
    {"GatewayPingProblem_NoDefaultPing",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_PROBLEM_PING_DEFAULT_FAILED},
    {"GatewayPingProblem_DefaultLatency",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_PROBLEM_DEFAULT_ABOVE_LATENCY},
    {"GatewayPingProblem_NoNonDefaultPing",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_PROBLEM_PING_NON_DEFAULT_FAILED},
    {"GatewayPingProblem_NonDefaultLatency",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_PROBLEM_NON_DEFAULT_ABOVE_LATENCY},
    {"SecureWifiProblem_None",
     IDS_NETWORK_DIAGNOSTICS_SECURE_WIFI_PROBLEM_NOT_SECURE},
    {"SecureWifiProblem_8021x",
     IDS_NETWORK_DIAGNOSTICS_SECURE_WIFI_PROBLEM_WEP_8021x},
    {"SecureWifiProblem_PSK",
     IDS_NETWORK_DIAGNOSTICS_SECURE_WIFI_PROBLEM_WEP_PSK},
    {"SecureWifiProblem_Unknown",
     IDS_NETWORK_DIAGNOSTICS_SECURE_WIFI_PROBLEM_UNKNOWN},
    {"DnsResolverProblem_NoNameServers",
     IDS_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PROBLEM_NO_NAME_SERVERS},
    {"DnsResolverProblem_MalformedNameServers",
     IDS_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PROBLEM_MALFORMED_NAME_SERVERS},
    {"DnsResolverProblem_EmptyNameServers",
     IDS_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PROBLEM_EMPTY_NAME_SERVERS},
    {"DnsLatencyProblem_FailedResolveHosts",
     IDS_NETWORK_DIAGNOSTICS_DNS_LATENCY_PROBLEM_FAILED_TO_RESOLVE_ALL_HOSTS},
    {"DnsLatencyProblem_LatencySlightlyAbove",
     IDS_NETWORK_DIAGNOSTICS_DNS_LATENCY_PROBLEM_SLIGHTLY_ABOVE_THRESHOLD},
    {"DnsLatencyProblem_LatencySignificantlyAbove",
     IDS_NETWORK_DIAGNOSTICS_DNS_LATENCY_PROBLEM_SIGNIFICANTLY_ABOVE_THRESHOLD},
    {"DnsResolutionProblem_FailedResolve",
     IDS_NETWORK_DIAGNOSTICS_DNS_RESOLUTION_PROBLEM_FAILED_TO_RESOLVE_HOST},
    {"FirewallProblem_DnsResolutionFailureRate",
     IDS_NETWORK_DIAGNOSTICS_FIREWALL_PROBLEM_DNS_RESOLUTION_FAILURE_RATE},
    {"FirewallProblem_FirewallDetected",
     IDS_NETWORK_DIAGNOSTICS_FIREWALL_PROBLEM_FIREWALL_DETECTED},
    {"FirewallProblem_FirewallSuspected",
     IDS_NETWORK_DIAGNOSTICS_FIREWALL_PROBLEM_FIREWALL_SUSPECTED},
    {"HttpsLatencyProblem_FailedDnsResolution",
     IDS_NETWORK_DIAGNOSTICS_HTTPS_LATENCY_PROBLEM_FAILED_DNS_RESOLUTIONS},
    {"HttpsLatencyProblem_FailedHttpsRequests",
     IDS_NETWORK_DIAGNOSTICS_HTTPS_LATENCY_PROBLEM_FAILED_HTTPS_REQUESTS},
    {"HttpsLatencyProblem_HighLatency",
     IDS_NETWORK_DIAGNOSTICS_HTTPS_LATENCY_PROBLEM_HIGH_LATENCY},
    {"HttpsLatencyProblem_VeryHighLatency",
     IDS_NETWORK_DIAGNOSTICS_HTTPS_LATENCY_PROBLEM_VERY_HIGH_LATENCY},
    {"ArcHttpProblem_FailedHttpRequests",
     IDS_NETWORK_DIAGNOSTICS_ARC_HTTP_PROBLEM_FAILED_HTTP_REQUESTS},
    {"ArcHttpProblem_HighLatency",
     IDS_NETWORK_DIAGNOSTICS_ARC_HTTP_PROBLEM_HIGH_LATENCY},
    {"ArcHttpProblem_VeryHighLatency",
     IDS_NETWORK_DIAGNOSTICS_ARC_HTTP_PROBLEM_VERY_HIGH_LATENCY},
    {"ArcRoutineProblem_InternalError",
     IDS_NETWORK_DIAGNOSTICS_ARC_ROUTINE_PROBLEM_INTERNAL_ERROR},
    {"ArcRoutineProblem_ArcNotRunning",
     IDS_NETWORK_DIAGNOSTICS_ARC_ROUTINE_PROBLEM_ARC_NOT_RUNNING},
    {"CaptivePortalProblem_NoActiveNetworks",
     IDS_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL_PROBLEM_NO_ACTIVE_NETWORKS},
    {"CaptivePortalProblem_UnknownPortalState",
     IDS_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL_PROBLEM_UNKNOWN_PORTAL_STATE},
    {"CaptivePortalProblem_PortalSuspected",
     IDS_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL_PROBLEM_PORTAL_SUSPECTED},
    {"CaptivePortalProblem_Portal",
     IDS_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL_PROBLEM_PORTAL},
    {"CaptivePortalProblem_ProxyAuthRequired",
     IDS_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL_PROBLEM_PROXY_AUTH_REQUIRED},
    {"CaptivePortalProblem_NoInternet",
     IDS_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL_PROBLEM_NO_INTERNET},
    {"VideoConferencingProblem_UdpFailure",
     IDS_NETWORK_DIAGNOSTICS_VIDEO_CONFERENCING_PROBLEM_UPD_FAILURE},
    {"VideoConferencingProblem_TcpFailure",
     IDS_NETWORK_DIAGNOSTICS_VIDEO_CONFERENCING_PROBLEM_TCP_FAILURE},
    {"VideoConferencingProblem_MediaFailure",
     IDS_NETWORK_DIAGNOSTICS_VIDEO_CONFERENCING_PROBLEM_MEDIA_FAILURE},
};

struct WebUiResource {
  const char* name;
  int id;
};

constexpr WebUiResource kResources[] = {
    {"test_canceled.png",
     IDR_ASH_WEBUI_COMMON_NETWORK_HEALTH_TEST_CANCELED_PNG},
    {"test_failed.png", IDR_ASH_WEBUI_COMMON_NETWORK_HEALTH_TEST_FAILED_PNG},
    {"test_not_run.png", IDR_ASH_WEBUI_COMMON_NETWORK_HEALTH_TEST_NOT_RUN_PNG},
    {"test_passed.png", IDR_ASH_WEBUI_COMMON_NETWORK_HEALTH_TEST_PASSED_PNG},
    {"test_warning.png", IDR_ASH_WEBUI_COMMON_NETWORK_HEALTH_TEST_WARNING_PNG},
};

struct StringMap {
  const char* name;
  const char* value;
};

}  // namespace

void AddResources(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedStrings(kLocalizedStrings);

  for (const auto& resource : kResources)
    html_source->AddResourcePath(resource.name, resource.id);
}

}  // namespace network_diagnostics
}  // namespace ash
