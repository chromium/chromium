// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/network_health_source.h"

#include <sstream>
#include <string>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/net/network_health/network_health_manager.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

constexpr char kNetworkDiagnosticsEntry[] = "network-diagnostics";

std::string FormatNetworkHealth(
    const chromeos::network_health::mojom::NetworkHealthStatePtr&
        network_health,
    bool scrub,
    bool include_guid_when_not_scrub) {
  std::ostringstream output;

  for (const auto& net : network_health->networks) {
    if (scrub) {
      output << "Name: " << ash::NetworkGuidId(net->guid.value_or("N/A"))
             << "\n";
    } else {
      output << "Name: " << net->name.value_or("N/A") << "\n";
    }

    if (!scrub && include_guid_when_not_scrub) {
      output << "GUID: " << net->guid.value_or("N/A") << "\n";
    }

    output << "Type: " << net->type << "\n";
    output << "State: " << net->state << "\n";
    output << "Portal State: " << net->portal_state << "\n";
    if (net->signal_strength) {
      output << "Signal Strength: "
             << base::NumberToString(net->signal_strength->value) << "\n";
    }
    if (net->signal_strength_stats) {
      output << "Signal Strength (Average): "
             << base::NumberToString(net->signal_strength_stats->average)
             << "\n";
      output << "Signal Strength (Deviation): "
             << base::NumberToString(net->signal_strength_stats->deviation)
             << "\n";
      output << "Signal Strength (Samples): [";
      auto& samples = net->signal_strength_stats->samples;
      for (uint16_t i = 0; i < samples.size(); i++) {
        output << base::NumberToString(samples[i]);
        if (i < samples.size() - 1)
          output << ",";
      }

      output << "]\n";
    }
    output << "MAC Address: " << net->mac_address.value_or("N/A") << "\n";

    // Automatic PII scrubbing does not work for IP addresses so manually scrub
    // them.
    if (!scrub) {
      output << "IPV4 Address: " << net->ipv4_address.value_or("N/A") << "\n";
      output << "IPV6 Addresses: "
             << (net->ipv6_addresses.size()
                     ? base::JoinString(net->ipv6_addresses, ", ")
                     : "N/A")
             << "\n";
    }

    output << "\n";
  }
  return output.str();
}

template <typename T>
std::string ProblemsToStr(T problems) {
  if (problems.size() == 0) {
    return "";
  }

  std::ostringstream output;
  for (uint16_t i = 0; i < problems.size(); i++) {
    output << problems[i];
    if (i != problems.size() - 1)
      output << ", ";
  }
  return output.str();
}

std::string GetProblemsString(
    const chromeos::network_diagnostics::mojom::RoutineProblemsPtr& problems) {
  using chromeos::network_diagnostics::mojom::RoutineProblems;
  std::string problemsStr;
  switch (problems->which()) {
    case RoutineProblems::Tag::kLanConnectivityProblems:
      problemsStr = ProblemsToStr(problems->get_lan_connectivity_problems());
      break;
    case RoutineProblems::Tag::kSignalStrengthProblems:
      problemsStr = ProblemsToStr(problems->get_signal_strength_problems());
      break;
    case RoutineProblems::Tag::kGatewayCanBePingedProblems:
      problemsStr =
          ProblemsToStr(problems->get_gateway_can_be_pinged_problems());
      break;
    case RoutineProblems::Tag::kHasSecureWifiConnectionProblems:
      problemsStr =
          ProblemsToStr(problems->get_has_secure_wifi_connection_problems());
      break;
    case RoutineProblems::Tag::kDnsResolverPresentProblems:
      problemsStr =
          ProblemsToStr(problems->get_dns_resolver_present_problems());
      break;
    case RoutineProblems::Tag::kDnsLatencyProblems:
      problemsStr = ProblemsToStr(problems->get_dns_latency_problems());
      break;
    case RoutineProblems::Tag::kDnsResolutionProblems:
      problemsStr = ProblemsToStr(problems->get_dns_resolution_problems());
      break;
    case RoutineProblems::Tag::kCaptivePortalProblems:
      problemsStr = ProblemsToStr(problems->get_captive_portal_problems());
      break;
    case RoutineProblems::Tag::kHttpFirewallProblems:
      problemsStr = ProblemsToStr(problems->get_http_firewall_problems());
      break;
    case RoutineProblems::Tag::kHttpsFirewallProblems:
      problemsStr = ProblemsToStr(problems->get_https_firewall_problems());
      break;
    case RoutineProblems::Tag::kHttpsLatencyProblems:
      problemsStr = ProblemsToStr(problems->get_https_latency_problems());
      break;
    case RoutineProblems::Tag::kVideoConferencingProblems:
      problemsStr = ProblemsToStr(problems->get_video_conferencing_problems());
      break;
    case RoutineProblems::Tag::kArcHttpProblems:
      problemsStr = ProblemsToStr(problems->get_arc_http_problems());
      break;
    case RoutineProblems::Tag::kArcDnsResolutionProblems:
      problemsStr = ProblemsToStr(problems->get_arc_dns_resolution_problems());
      break;
    case RoutineProblems::Tag::kArcPingProblems:
      problemsStr = ProblemsToStr(problems->get_arc_ping_problems());
      break;
  }
  return problemsStr;
}

}  // namespace

const char kNetworkHealthSnapshotEntry[] = "network-health-snapshot";

std::string FormatNetworkDiagnosticResults(
    const base::flat_map<
        chromeos::network_diagnostics::mojom::RoutineType,
        chromeos::network_diagnostics::mojom::RoutineResultPtr>& results,
    bool scrub) {
  std::ostringstream output;

  for (const auto& result : results) {
    output << "Routine: " << result.first << "\n";
    output << "Verdict: " << result.second->verdict << "\n";
    output << "Timestamp: " << result.second->timestamp << "\n";
    output << "Source: " << result.second->source << "\n";

    auto problems = GetProblemsString(result.second->problems);
    if (!problems.empty())
      output << "Problems: " << problems << "\n";

    output << "\n";
  }
  return output.str();
}

NetworkHealthSource::NetworkHealthSource(bool scrub,
                                         bool include_guid_when_not_scrub)
    : SystemLogsSource("NetworkHealth"),
      scrub_(scrub),
      include_guid_when_not_scrub_(include_guid_when_not_scrub) {
  ash::network_health::NetworkHealthManager::GetInstance()->BindHealthReceiver(
      network_health_service_.BindNewPipeAndPassReceiver());
  ash::network_health::NetworkHealthManager::GetInstance()
      ->BindDiagnosticsReceiver(
          network_diagnostics_service_.BindNewPipeAndPassReceiver());
}

NetworkHealthSource::~NetworkHealthSource() {}

void NetworkHealthSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());
  callback_ = std::move(callback);
  network_health_service_->GetHealthSnapshot(
      base::BindOnce(&NetworkHealthSource::OnNetworkHealthReceived,
                     weak_factory_.GetWeakPtr()));
  network_diagnostics_service_->GetAllResults(
      base::BindOnce(&NetworkHealthSource::OnNetworkDiagnosticResultsReceived,
                     weak_factory_.GetWeakPtr()));
}

void NetworkHealthSource::OnNetworkHealthReceived(
    chromeos::network_health::mojom::NetworkHealthStatePtr network_health) {
  network_health_response_ =
      FormatNetworkHealth(network_health, scrub_, include_guid_when_not_scrub_);
  CheckIfDone();
}

void NetworkHealthSource::OnNetworkDiagnosticResultsReceived(
    base::flat_map<chromeos::network_diagnostics::mojom::RoutineType,
                   chromeos::network_diagnostics::mojom::RoutineResultPtr>
        results) {
  network_diagnostics_response_ =
      FormatNetworkDiagnosticResults(results, scrub_);
  CheckIfDone();
}

void NetworkHealthSource::CheckIfDone() {
  if (!network_health_response_.has_value() ||
      !network_diagnostics_response_.has_value()) {
    return;
  }

  auto response = std::make_unique<SystemLogsResponse>();
  (*response)[kNetworkHealthSnapshotEntry] =
      std::move(network_health_response_.value());
  (*response)[kNetworkDiagnosticsEntry] =
      std::move(network_diagnostics_response_.value());

  network_health_response_.reset();
  network_diagnostics_response_.reset();

  std::move(callback_).Run(std::move(response));
}

}  // namespace system_logs
