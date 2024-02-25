// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/net/network_diagnostics/arc_dns_resolution_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_util.h"
#include "net/dns/public/dns_protocol.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

// These hostnames were chosen because they need to be resolved for a
// successful ARC provisioning step.
constexpr char kHostname1[] = "www.googleapis.com";
constexpr char kHostname2[] = "android.clients.google.com";
constexpr char kHostname3[] = "android.googleapis.com";

}  // namespace

ArcDnsResolutionRoutine::ArcDnsResolutionRoutine(
    mojom::RoutineCallSource source)
    : NetworkDiagnosticsRoutine(source) {
  hostnames_to_resolve_dns_ = {kHostname1, kHostname2, kHostname3};
}

ArcDnsResolutionRoutine::~ArcDnsResolutionRoutine() = default;

mojom::RoutineType ArcDnsResolutionRoutine::Type() {
  return mojom::RoutineType::kArcDnsResolution;
}

void ArcDnsResolutionRoutine::Run() {
  AttemptNextQuery();
}

void ArcDnsResolutionRoutine::AttemptNextQuery() {
  // If no more hostnames to resolve, report success and analyze results.
  if (hostnames_to_resolve_dns_.empty()) {
    successfully_resolved_hostnames_ = true;
    AnalyzeResultsAndExecuteCallback();
    return;
  }

  std::string hostname = hostnames_to_resolve_dns_.back();
  hostnames_to_resolve_dns_.pop_back();

  // Call the DnsResolutionTest API from the instance of NetInstance.
  arc::mojom::NetInstance* net_instance = GetNetInstance();
  if (net_instance) {
    net_instance->DnsResolutionTest(
        "" /* default network */, hostname,
        base::BindOnce(&ArcDnsResolutionRoutine::OnQueryComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

arc::mojom::NetInstance* ArcDnsResolutionRoutine::GetNetInstance() {
  // If |net_instance_| is not already set for testing purposes, get instance
  // of NetInstance service.
  if (net_instance_) {
    return net_instance_;
  }

  // Call the singleton for ArcServiceManager and check if it is null.
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    failed_to_get_arc_service_manager_ = true;
    AnalyzeResultsAndExecuteCallback();
    return nullptr;
  }

  // Get an instance of the NetInstance service and check if it is null.
  auto* arc_bridge_service = arc_service_manager->arc_bridge_service();
  arc::mojom::NetInstance* net_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service->net(), DnsResolutionTest);
  if (!net_instance) {
    failed_to_get_net_instance_service_for_dns_resolution_test_ = true;
    AnalyzeResultsAndExecuteCallback();
    return nullptr;
  }
  return net_instance;
}

void ArcDnsResolutionRoutine::OnQueryComplete(
    arc::mojom::ArcDnsResolutionTestResultPtr result) {
  if (!result->is_successful ||
      result->response_code != net::dns_protocol::kRcodeNOERROR) {
    successfully_resolved_hostnames_ = false;
    AnalyzeResultsAndExecuteCallback();
    return;
  }

  max_latency_ = std::max(max_latency_, result->duration_ms);
  AttemptNextQuery();
}

void ArcDnsResolutionRoutine::AnalyzeResultsAndExecuteCallback() {
  if (!successfully_resolved_hostnames_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.push_back(mojom::ArcDnsResolutionProblem::kFailedDnsQueries);
  } else if (failed_to_get_arc_service_manager_) {
    set_verdict(mojom::RoutineVerdict::kNotRun);
    problems_.push_back(
        mojom::ArcDnsResolutionProblem::kFailedToGetArcServiceManager);
  } else if (failed_to_get_net_instance_service_for_dns_resolution_test_) {
    set_verdict(mojom::RoutineVerdict::kNotRun);
    problems_.push_back(mojom::ArcDnsResolutionProblem::
                            kFailedToGetNetInstanceForDnsResolutionTest);
  } else if (max_latency_ <= util::kDnsProblemLatencyMs &&
             max_latency_ > util::kDnsPotentialProblemLatencyMs) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.push_back(mojom::ArcDnsResolutionProblem::kHighLatency);
  } else if (max_latency_ > util::kDnsProblemLatencyMs) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.push_back(mojom::ArcDnsResolutionProblem::kVeryHighLatency);
  } else {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  }
  set_problems(mojom::RoutineProblems::NewArcDnsResolutionProblems(problems_));
  ExecuteCallback();
}

}  // namespace network_diagnostics
}  // namespace ash
