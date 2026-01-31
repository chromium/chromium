// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/google_services_connectivity_routine.h"

#include <array>
#include <optional>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/net/network_diagnostics/google_services_connectivity_routine_util.h"
#include "chrome/browser/ash/net/network_diagnostics/hosts_connectivity_diagnostics.pb.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"

namespace ash::network_diagnostics {

namespace {

// Debugd constants used by the connectivity tool when parsing options.
constexpr char kDebugdTimeoutStr[] = "timeout";
constexpr char kDebugdMaxErrorsStr[] = "max_errors";
constexpr char kDebugdProxyStr[] = "proxy";

// Maximum time in seconds per hostname connection. The connectivity tool
// applies this timeout to each individual host test, not the entire routine.
constexpr char kTimeoutSecNum[] = "10";
// Maximum number of errors the tool is allowed to identify after which it must
// return the result.
constexpr char kMaxErrorsNum[] = "5";
// Sets up the connectivity tool to automatically determine proxy for each
// hostname (eg. from a PAC file).
constexpr char kSystemProxy[] = "system";

namespace mojom = ::chromeos::network_diagnostics::mojom;

// Returns true if the created problem has all required fields per proto field
// availability contract. Malformed problems should be skipped to ensure
// downstream consumers never receive invalid data.
bool IsValidProblem(
    const mojom::GoogleServicesConnectivityProblemPtr& problem) {
  if (problem->is_connection_error()) {
    const auto& conn = problem->get_connection_error();
    const auto& info = conn->connection_info;

    // Check required fields: error_message always, hostname for most types.
    const bool missing_error_message =
        info->error_details->error_message.empty();
    const bool missing_hostname =
        conn->problem_type !=
            mojom::GoogleServicesConnectivityProblemType::kInternalError &&
        info->hostname.empty();

    if (missing_error_message || missing_hostname) {
      DLOG(WARNING) << "Skipping malformed connection_error:"
                    << (missing_error_message ? " error_message" : "")
                    << (missing_hostname ? " hostname" : "") << " required."
                    << " problem_type=" << conn->problem_type
                    << ", hostname=" << info->hostname
                    << ", error_message=" << info->error_details->error_message;
      return false;
    }
    return true;
  }

  if (problem->is_proxy_connection_error()) {
    const auto& proxy_conn = problem->get_proxy_connection_error();
    const auto& info = proxy_conn->connection_info;

    // Check required fields: error_message, hostname, and proxy.
    const bool missing_error_message =
        info->error_details->error_message.empty();
    const bool missing_hostname = info->hostname.empty();
    const bool missing_proxy = proxy_conn->proxy.empty();

    if (missing_error_message || missing_hostname || missing_proxy) {
      DLOG(WARNING) << "Skipping malformed proxy_connection_error:"
                    << (missing_error_message ? " error_message" : "")
                    << (missing_hostname ? " hostname" : "")
                    << (missing_proxy ? " proxy" : "") << " required."
                    << " problem_type=" << proxy_conn->problem_type
                    << ", hostname=" << info->hostname
                    << ", proxy=" << proxy_conn->proxy
                    << ", error_message=" << info->error_details->error_message;
      return false;
    }
    return true;
  }

  // no_valid_proxy_error: hostname and proxy are optional, error_message
  // required.
  const auto& no_proxy = problem->get_no_valid_proxy_error();
  if (no_proxy->error_details->error_message.empty()) {
    DLOG(WARNING) << "Skipping malformed no_valid_proxy_error: error_message "
                     "required."
                  << " hostname=" << no_proxy->hostname.value_or("(empty)")
                  << ", proxy=" << no_proxy->proxy.value_or("(empty)");
    return false;
  }
  return true;
}

// Creates a connection_error union variant for TestConnectivity request
// failures (empty response, invalid protobuf). These are Chrome-side errors
// that occur before/during the D-Bus call, not proto result codes.
mojom::GoogleServicesConnectivityProblemPtr
CreateTestConnectivityRequestFailedError(std::string_view error_message) {
  return mojom::GoogleServicesConnectivityProblem::NewConnectionError(
      mojom::GoogleServicesConnectivityConnectionError::New(
          mojom::GoogleServicesConnectivityProblemType::kInternalError,
          /*proxy=*/std::nullopt,
          mojom::GoogleServicesConnectivityConnectionErrorInfo::New(
              /*hostname=*/"",
              mojom::GoogleServicesConnectivityErrorDetails::New(
                  std::string(error_message),
                  /*resolution_message=*/std::nullopt),
              /*timestamp_start=*/std::nullopt,
              /*timestamp_end=*/std::nullopt)));
}

}  // namespace

GoogleServicesConnectivityRoutine::GoogleServicesConnectivityRoutine(
    mojom::RoutineCallSource source,
    DebugDaemonClient* debug_daemon_client)
    : NetworkDiagnosticsRoutine(source),
      debug_daemon_client_(CHECK_DEREF(debug_daemon_client)) {
  set_verdict(mojom::RoutineVerdict::kNotRun);
}

GoogleServicesConnectivityRoutine::~GoogleServicesConnectivityRoutine() =
    default;

mojom::RoutineType GoogleServicesConnectivityRoutine::Type() {
  return mojom::RoutineType::kGoogleServicesConnectivity;
}

bool GoogleServicesConnectivityRoutine::CanRun() {
  return base::FeatureList::IsEnabled(
      ash::features::kGoogleServicesConnectivityRoutine);
}

void GoogleServicesConnectivityRoutine::Run() {
  CHECK(CanRun());
  // TODO(crbug.com/463098734): define a list of Google services in the util
  // file according to the requirements document.
  static constexpr auto kGoogleHosts = std::to_array<std::string_view>(
      {"clients1.google.com", "clients2.google.com", "clients3.google.com",
       "clients4.google.com", "clients5.google.com"});

  const base::flat_map<std::string, std::string> options = {
      {kDebugdTimeoutStr, kTimeoutSecNum},
      {kDebugdMaxErrorsStr, kMaxErrorsNum},
      {kDebugdProxyStr, kSystemProxy},
  };
  debug_daemon_client_->TestHostsConnectivity(
      std::vector<std::string>(kGoogleHosts.begin(), kGoogleHosts.end()),
      options,
      base::BindOnce(
          &GoogleServicesConnectivityRoutine::OnGetHostsConnectivityResult,
          weak_factory_.GetWeakPtr()));
}

void GoogleServicesConnectivityRoutine::OnGetHostsConnectivityResult(
    const std::vector<uint8_t>& response) {
  if (response.empty()) {
    problems_.push_back(CreateTestConnectivityRequestFailedError(
        kGoogleServicesConnectivityFailedToExecuteError));
  } else {
    ParseConnectivityResponse(response);
  }
  AnalyzeResultsAndExecuteCallback();
}

void GoogleServicesConnectivityRoutine::ParseConnectivityResponse(
    const std::vector<uint8_t>& proto_response) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  if (!response.ParseFromArray(proto_response.data(), proto_response.size())) {
    problems_.push_back(CreateTestConnectivityRequestFailedError(
        kGoogleServicesConnectivityInvalidProtobufError));
    return;
  }

  for (const auto& entry : response.connectivity_results()) {
    const auto result_code = entry.result_code();

    // Skip successful and unspecified results. If there are no results in
    // `problems_`, the routine will be considered as successfully passed.
    if (IsSuccessfulOrUnspecifiedResult(result_code)) {
      continue;
    }

    // Extract common fields.
    auto error_details = mojom::GoogleServicesConnectivityErrorDetails::New(
        entry.error_message(),
        ToOptionalStrIfNonEmpty(entry.resolution_message()));

    mojom::GoogleServicesConnectivityProblemPtr problem;
    if (IsNoValidProxyError(result_code)) {
      // NO_VALID_PROXY: no timestamps, hostname/proxy are optional.
      problem = mojom::GoogleServicesConnectivityProblem::NewNoValidProxyError(
          mojom::GoogleServicesConnectivityNoValidProxyError::New(
              ToOptionalStrIfNonEmpty(entry.hostname()),
              ToOptionalStrIfNonEmpty(entry.proxy()),
              std::move(error_details)));
    } else {
      // Connection errors have timestamps.
      std::optional<base::Time> timestamp_start;
      if (entry.has_timestamp_start()) {
        timestamp_start = base::Time::FromMillisecondsSinceUnixEpoch(
            static_cast<int64_t>(entry.timestamp_start()));
      }

      std::optional<base::Time> timestamp_end;
      if (entry.has_timestamp_end()) {
        timestamp_end = base::Time::FromMillisecondsSinceUnixEpoch(
            static_cast<int64_t>(entry.timestamp_end()));
      }

      auto connection_info =
          mojom::GoogleServicesConnectivityConnectionErrorInfo::New(
              entry.hostname(), std::move(error_details),
              std::move(timestamp_start), std::move(timestamp_end));

      if (IsProxyConnectionError(result_code)) {
        // PROXY_DNS_RESOLUTION_ERROR or PROXY_CONNECTION_FAILURE.
        problem =
            mojom::GoogleServicesConnectivityProblem::NewProxyConnectionError(
                mojom::GoogleServicesConnectivityProxyConnectionError::New(
                    ToProxyProblemType(result_code), entry.proxy(),
                    std::move(connection_info)));
      } else {
        // All other connection errors: proxy is optional.
        problem = mojom::GoogleServicesConnectivityProblem::NewConnectionError(
            mojom::GoogleServicesConnectivityConnectionError::New(
                ToConnectivityProblemType(result_code),
                ToOptionalStrIfNonEmpty(entry.proxy()),
                std::move(connection_info)));
      }
    }

    // Validate the created problem before adding to results.
    // Skip malformed problems to ensure consumers never receive invalid data.
    if (IsValidProblem(problem)) {
      problems_.push_back(std::move(problem));
    }
  }
}

void GoogleServicesConnectivityRoutine::AnalyzeResultsAndExecuteCallback() {
  set_verdict(problems_.empty() ? mojom::RoutineVerdict::kNoProblem
                                : mojom::RoutineVerdict::kProblem);

  set_problems(mojom::RoutineProblems::NewGoogleServicesConnectivityProblems(
      std::move(problems_)));
  ExecuteCallback();
}

}  // namespace ash::network_diagnostics
