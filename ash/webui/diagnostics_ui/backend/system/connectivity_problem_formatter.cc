// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/system/connectivity_problem_formatter.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"

namespace ash::diagnostics {

namespace {

std::string FormatTime(const base::Time& time) {
  return base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(time));
}

constexpr std::string_view kStatusFailPrefix = "  Status: FAIL - ";
constexpr std::string_view kErrorPrefix = "  Error: ";
constexpr std::string_view kResolutionPrefix = "  Resolution: ";
constexpr std::string_view kProxyPrefix = "  Proxy: ";

// Strips the leading "k" from a mojom enum stringification.
// e.g. "kConnectionFailure" -> "ConnectionFailure".
template <typename T>
std::string ProblemTypeToString(T problem_type) {
  std::string name = base::ToString(problem_type);
  return std::string(base::RemovePrefix(name, "k").value_or(name));
}

void AppendConnectionInfoLines(
    std::string& out,
    const chromeos::network_diagnostics::mojom::
        GoogleServicesConnectivityConnectionErrorInfoPtr& info) {
  if (info->timestamp_start.has_value()) {
    base::StrAppend(
        &out, {"  Start: ", FormatTime(info->timestamp_start.value()), "\n"});
  }
  if (info->timestamp_end.has_value()) {
    base::StrAppend(&out,
                    {"  End: ", FormatTime(info->timestamp_end.value()), "\n"});
  }
  base::StrAppend(&out,
                  {kErrorPrefix, info->error_details->error_message, "\n"});
  if (info->error_details->resolution_message.has_value()) {
    base::StrAppend(&out,
                    {kResolutionPrefix,
                     info->error_details->resolution_message.value(), "\n"});
  }
}

std::string FormatConnectionError(
    const chromeos::network_diagnostics::mojom::
        GoogleServicesConnectivityConnectionErrorPtr& error) {
  std::string out;
  if (!error->connection_info->hostname.empty()) {
    base::StrAppend(&out, {error->connection_info->hostname, ":\n"});
  }
  base::StrAppend(&out, {kStatusFailPrefix,
                         ProblemTypeToString(error->problem_type), "\n"});
  if (error->proxy.has_value()) {
    base::StrAppend(&out, {kProxyPrefix, error->proxy.value(), "\n"});
  }
  AppendConnectionInfoLines(out, error->connection_info);
  return out;
}

std::string FormatProxyConnectionError(
    const chromeos::network_diagnostics::mojom::
        GoogleServicesConnectivityProxyConnectionErrorPtr& error) {
  std::string out =
      base::StrCat({error->connection_info->hostname, ":\n", kStatusFailPrefix,
                    ProblemTypeToString(error->problem_type), "\n",
                    kProxyPrefix, error->proxy, "\n"});
  AppendConnectionInfoLines(out, error->connection_info);
  return out;
}

std::string FormatNoValidProxyError(
    const chromeos::network_diagnostics::mojom::
        GoogleServicesConnectivityNoValidProxyErrorPtr& error) {
  std::string out = base::StrCat(
      {error->proxy.has_value() ? error->proxy.value() : "Proxy error", ":\n"});
  if (error->hostname.has_value()) {
    base::StrAppend(&out, {"  Hostname: ", error->hostname.value(), "\n"});
  }
  base::StrAppend(&out, {kStatusFailPrefix, "No Valid Proxy\n", kErrorPrefix,
                         error->error_details->error_message, "\n"});
  if (error->error_details->resolution_message.has_value()) {
    base::StrAppend(&out,
                    {kResolutionPrefix,
                     error->error_details->resolution_message.value(), "\n"});
  }
  return out;
}

}  // namespace

std::string FormatConnectivityProblems(
    const std::vector<chromeos::network_diagnostics::mojom::
                          GoogleServicesConnectivityProblemPtr>& problems) {
  std::vector<std::string> blocks;
  blocks.reserve(problems.size());
  for (const auto& problem : problems) {
    if (problem->is_connection_error()) {
      blocks.emplace_back(
          FormatConnectionError(problem->get_connection_error()));
    } else if (problem->is_proxy_connection_error()) {
      blocks.emplace_back(
          FormatProxyConnectionError(problem->get_proxy_connection_error()));
    } else if (problem->is_no_valid_proxy_error()) {
      blocks.emplace_back(
          FormatNoValidProxyError(problem->get_no_valid_proxy_error()));
    } else {
      NOTREACHED();
    }
  }
  return base::JoinString(blocks, "\n");
}

}  // namespace ash::diagnostics
