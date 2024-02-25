// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/net/network_diagnostics/arc_http_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_util.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

constexpr int kTotalHostsToQuery = 3;
// The length of a random eight letter prefix.
constexpr int kHostPrefixLength = 8;
constexpr char kHttpScheme[] = "http://";

// Requests taking longer than 1000 ms are problematic.
constexpr int kProblemLatencyMs = 1000;
// Requests lasting between 500 ms and 1000 ms are potentially problematic.
constexpr int kPotentialProblemLatencyMs = 500;

}  // namespace

ArcHttpRoutine::ArcHttpRoutine(mojom::RoutineCallSource source)
    : NetworkDiagnosticsRoutine(source),
      hostnames_to_request_http_(
          util::GetRandomHostsWithSchemeAndGenerate204Path(kTotalHostsToQuery,
                                                           kHostPrefixLength,
                                                           kHttpScheme)) {}

ArcHttpRoutine::~ArcHttpRoutine() = default;

mojom::RoutineType ArcHttpRoutine::Type() {
  return mojom::RoutineType::kArcHttp;
}

void ArcHttpRoutine::Run() {
  AttemptNextRequest();
}

void ArcHttpRoutine::AttemptNextRequest() {
  // If no more hostnames to request, report success and analyze results.
  if (hostnames_to_request_http_.empty()) {
    successfully_requested_targets_ = true;
    AnalyzeResultsAndExecuteCallback();
    return;
  }

  auto url = GURL(hostnames_to_request_http_.back());
  hostnames_to_request_http_.pop_back();

  // Call the HttpTest API from the instance of NetInstance.
  arc::mojom::NetInstance* net_instance = GetNetInstance();
  if (net_instance) {
    net_instance->HttpTest("" /* default network */, url,
                           base::BindOnce(&ArcHttpRoutine::OnRequestComplete,
                                          weak_ptr_factory_.GetWeakPtr()));
  }
}

arc::mojom::NetInstance* ArcHttpRoutine::GetNetInstance() {
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
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service->net(), HttpTest);
  if (!net_instance) {
    failed_to_get_net_instance_service_for_http_test_ = true;
    AnalyzeResultsAndExecuteCallback();
    return nullptr;
  }
  return net_instance;
}

void ArcHttpRoutine::OnRequestComplete(
    arc::mojom::ArcHttpTestResultPtr result) {
  if (!result->is_successful ||
      // We generated path to a page that returns 204 response.
      result->status_code != net::HttpStatusCode::HTTP_NO_CONTENT) {
    successfully_requested_targets_ = false;
    AnalyzeResultsAndExecuteCallback();
    return;
  }

  max_latency_ = std::max(max_latency_, result->duration_ms);
  AttemptNextRequest();
}

void ArcHttpRoutine::AnalyzeResultsAndExecuteCallback() {
  if (!successfully_requested_targets_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.push_back(mojom::ArcHttpProblem::kFailedHttpRequests);
  } else if (failed_to_get_arc_service_manager_) {
    set_verdict(mojom::RoutineVerdict::kNotRun);
    problems_.push_back(mojom::ArcHttpProblem::kFailedToGetArcServiceManager);
  } else if (failed_to_get_net_instance_service_for_http_test_) {
    set_verdict(mojom::RoutineVerdict::kNotRun);
    problems_.push_back(
        mojom::ArcHttpProblem::kFailedToGetNetInstanceForHttpTest);
  } else if (max_latency_ <= kProblemLatencyMs &&
             max_latency_ > kPotentialProblemLatencyMs) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.push_back(mojom::ArcHttpProblem::kHighLatency);
  } else if (max_latency_ > kProblemLatencyMs) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.push_back(mojom::ArcHttpProblem::kVeryHighLatency);
  } else {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  }
  set_problems(mojom::RoutineProblems::NewArcHttpProblems(problems_));
  ExecuteCallback();
}

}  // namespace network_diagnostics
}  // namespace ash
