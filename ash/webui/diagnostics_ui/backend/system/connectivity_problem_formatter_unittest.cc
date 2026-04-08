// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/system/connectivity_problem_formatter.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::diagnostics {
namespace {

using chromeos::network_diagnostics::mojom::
    GoogleServicesConnectivityConnectionError;
using chromeos::network_diagnostics::mojom::
    GoogleServicesConnectivityConnectionErrorInfo;
using chromeos::network_diagnostics::mojom::
    GoogleServicesConnectivityErrorDetails;
using chromeos::network_diagnostics::mojom::
    GoogleServicesConnectivityNoValidProxyError;
using chromeos::network_diagnostics::mojom::GoogleServicesConnectivityProblem;
using chromeos::network_diagnostics::mojom::
    GoogleServicesConnectivityProblemPtr;
using chromeos::network_diagnostics::mojom::
    GoogleServicesConnectivityProblemType;
using chromeos::network_diagnostics::mojom::
    GoogleServicesConnectivityProxyConnectionError;
using chromeos::network_diagnostics::mojom::
    GoogleServicesConnectivityProxyProblemType;

constexpr std::string_view kTestHostname = "clients1.google.com";
constexpr std::string_view kTestProxy = "proxy.corp:8080";
constexpr std::string_view kTestErrorMsg = "Connection refused";

GoogleServicesConnectivityProblemPtr MakeConnectionError(
    std::string_view hostname,
    GoogleServicesConnectivityProblemType problem_type,
    std::string_view error_message,
    std::optional<std::string> proxy = std::nullopt,
    std::optional<base::Time> timestamp_start = std::nullopt,
    std::optional<base::Time> timestamp_end = std::nullopt,
    std::optional<std::string> resolution_message = std::nullopt) {
  auto error_details = GoogleServicesConnectivityErrorDetails::New(
      std::string(error_message), resolution_message);
  auto connection_info = GoogleServicesConnectivityConnectionErrorInfo::New(
      std::string(hostname), std::move(error_details), timestamp_start,
      timestamp_end);
  auto connection_error = GoogleServicesConnectivityConnectionError::New(
      problem_type, proxy, std::move(connection_info));
  return GoogleServicesConnectivityProblem::NewConnectionError(
      std::move(connection_error));
}

GoogleServicesConnectivityProblemPtr MakeProxyConnectionError(
    std::string_view hostname,
    std::string_view proxy,
    GoogleServicesConnectivityProxyProblemType problem_type,
    std::string_view error_message,
    std::optional<base::Time> timestamp_start = std::nullopt,
    std::optional<base::Time> timestamp_end = std::nullopt,
    std::optional<std::string> resolution_message = std::nullopt) {
  auto error_details = GoogleServicesConnectivityErrorDetails::New(
      std::string(error_message), resolution_message);
  auto connection_info = GoogleServicesConnectivityConnectionErrorInfo::New(
      std::string(hostname), std::move(error_details), timestamp_start,
      timestamp_end);
  auto proxy_error = GoogleServicesConnectivityProxyConnectionError::New(
      problem_type, std::string(proxy), std::move(connection_info));
  return GoogleServicesConnectivityProblem::NewProxyConnectionError(
      std::move(proxy_error));
}

GoogleServicesConnectivityProblemPtr MakeNoValidProxyError(
    std::string_view error_message,
    std::optional<std::string> hostname = std::nullopt,
    std::optional<std::string> proxy = std::nullopt,
    std::optional<std::string> resolution_message = std::nullopt) {
  auto error_details = GoogleServicesConnectivityErrorDetails::New(
      std::string(error_message), resolution_message);
  auto no_proxy_error = GoogleServicesConnectivityNoValidProxyError::New(
      hostname, proxy, std::move(error_details));
  return GoogleServicesConnectivityProblem::NewNoValidProxyError(
      std::move(no_proxy_error));
}

// Returns the expected label for a problem type after "k" prefix stripping.
std::string ExpectedLabel(GoogleServicesConnectivityProblemType type) {
  std::string name = base::ToString(type);
  return std::string(base::RemovePrefix(name, "k").value_or(name));
}

std::string ExpectedLabel(GoogleServicesConnectivityProxyProblemType type) {
  std::string name = base::ToString(type);
  return std::string(base::RemovePrefix(name, "k").value_or(name));
}

TEST(ConnectivityProblemFormatterTest, EmptyProblems) {
  std::vector<GoogleServicesConnectivityProblemPtr> problems;
  EXPECT_EQ("", FormatConnectivityProblems(problems));
}

TEST(ConnectivityProblemFormatterTest, ConnectionErrorMinimal) {
  std::vector<GoogleServicesConnectivityProblemPtr> problems;
  problems.emplace_back(MakeConnectionError(
      kTestHostname, GoogleServicesConnectivityProblemType::kConnectionFailure,
      kTestErrorMsg));
  const std::string result = FormatConnectivityProblems(problems);

  const std::string expected =
      "clients1.google.com:\n"
      "  Status: FAIL - ConnectionFailure\n"
      "  Error: Connection refused\n";
  EXPECT_EQ(expected, result);
}

TEST(ConnectivityProblemFormatterTest, ConnectionErrorAllFields) {
  base::Time start_time;
  ASSERT_TRUE(base::Time::FromString("2026-02-19 10:29:40 UTC", &start_time));
  base::Time end_time;
  ASSERT_TRUE(base::Time::FromString("2026-02-19 10:29:41 UTC", &end_time));

  std::vector<GoogleServicesConnectivityProblemPtr> problems;
  problems.emplace_back(MakeConnectionError(
      kTestHostname, GoogleServicesConnectivityProblemType::kDnsResolutionError,
      "DNS lookup failed",
      /*proxy=*/std::string(kTestProxy),
      /*timestamp_start=*/start_time,
      /*timestamp_end=*/end_time,
      /*resolution_message=*/"Check DNS settings"));
  const std::string result = FormatConnectivityProblems(problems);

  EXPECT_NE(std::string::npos, result.find("clients1.google.com:\n"));
  EXPECT_NE(std::string::npos,
            result.find("  Status: FAIL - DnsResolutionError\n"));
  EXPECT_NE(std::string::npos, result.find("  Proxy: proxy.corp:8080\n"));
  EXPECT_NE(std::string::npos, result.find("  Start: "));
  EXPECT_NE(std::string::npos, result.find("  End: "));
  EXPECT_NE(std::string::npos, result.find("  Error: DNS lookup failed\n"));
  EXPECT_NE(std::string::npos,
            result.find("  Resolution: Check DNS settings\n"));
  // Verify absence of fields not expected.
  EXPECT_EQ(std::string::npos, result.find("Hostname:"));
}

TEST(ConnectivityProblemFormatterTest, ProxyConnectionError) {
  std::vector<GoogleServicesConnectivityProblemPtr> problems;
  problems.emplace_back(MakeProxyConnectionError(
      kTestHostname, "squid.corp:3128",
      GoogleServicesConnectivityProxyProblemType::kProxyConnectionFailure,
      "Proxy connection refused"));
  const std::string result = FormatConnectivityProblems(problems);

  const std::string expected =
      "clients1.google.com:\n"
      "  Status: FAIL - ProxyConnectionFailure\n"
      "  Proxy: squid.corp:3128\n"
      "  Error: Proxy connection refused\n";
  EXPECT_EQ(expected, result);
}

TEST(ConnectivityProblemFormatterTest, NoValidProxyErrorWithProxy) {
  std::vector<GoogleServicesConnectivityProblemPtr> problems;
  problems.emplace_back(
      MakeNoValidProxyError("Invalid proxy URL",
                            /*hostname=*/std::nullopt,
                            /*proxy=*/"bad-proxy:9999",
                            /*resolution_message=*/"Fix proxy configuration"));
  const std::string result = FormatConnectivityProblems(problems);

  const std::string expected =
      "bad-proxy:9999:\n"
      "  Status: FAIL - No Valid Proxy\n"
      "  Error: Invalid proxy URL\n"
      "  Resolution: Fix proxy configuration\n";
  EXPECT_EQ(expected, result);
}

TEST(ConnectivityProblemFormatterTest, NoValidProxyErrorWithHostname) {
  std::vector<GoogleServicesConnectivityProblemPtr> problems;
  problems.emplace_back(
      MakeNoValidProxyError("No proxy available for host",
                            /*hostname=*/"clients3.google.com",
                            /*proxy=*/std::nullopt));
  const std::string result = FormatConnectivityProblems(problems);

  const std::string expected =
      "Proxy error:\n"
      "  Hostname: clients3.google.com\n"
      "  Status: FAIL - No Valid Proxy\n"
      "  Error: No proxy available for host\n";
  EXPECT_EQ(expected, result);
}

TEST(ConnectivityProblemFormatterTest, MixedErrors) {
  std::vector<GoogleServicesConnectivityProblemPtr> problems;
  problems.emplace_back(MakeConnectionError(
      "host-a.example.com",
      GoogleServicesConnectivityProblemType::kConnectionTimeout, "Timed out"));
  problems.emplace_back(MakeProxyConnectionError(
      "host-b.example.com", "squid:3128",
      GoogleServicesConnectivityProxyProblemType::kProxyDnsResolutionError,
      "Cannot resolve proxy"));
  problems.emplace_back(MakeNoValidProxyError("No proxy available",
                                              /*hostname=*/std::nullopt,
                                              /*proxy=*/"bad-proxy"));
  const std::string result = FormatConnectivityProblems(problems);

  // Blocks are separated by "\n" (from JoinString).
  EXPECT_NE(std::string::npos,
            result.find("host-a.example.com:\n"
                        "  Status: FAIL - ConnectionTimeout\n"
                        "  Error: Timed out\n"));
  EXPECT_NE(std::string::npos,
            result.find("host-b.example.com:\n"
                        "  Status: FAIL - ProxyDnsResolutionError\n"
                        "  Proxy: squid:3128\n"
                        "  Error: Cannot resolve proxy\n"));
  EXPECT_NE(std::string::npos, result.find("bad-proxy:\n"
                                           "  Status: FAIL - No Valid Proxy\n"
                                           "  Error: No proxy available\n"));
}

TEST(ConnectivityProblemFormatterTest, AllProblemTypeLabels) {
  constexpr GoogleServicesConnectivityProblemType kAllTypes[] = {
      GoogleServicesConnectivityProblemType::kInternalError,
      GoogleServicesConnectivityProblemType::kUnknownError,
      GoogleServicesConnectivityProblemType::kConnectionFailure,
      GoogleServicesConnectivityProblemType::kConnectionTimeout,
      GoogleServicesConnectivityProblemType::kDnsResolutionError,
      GoogleServicesConnectivityProblemType::kSSLConnectionError,
      GoogleServicesConnectivityProblemType::kPeerCertificateError,
      GoogleServicesConnectivityProblemType::kHttpError,
      GoogleServicesConnectivityProblemType::kNoNetworkError,
  };
  for (auto type : kAllTypes) {
    SCOPED_TRACE(base::ToString(type));
    std::vector<GoogleServicesConnectivityProblemPtr> problems;
    problems.emplace_back(MakeConnectionError("host", type, "err"));
    const std::string result = FormatConnectivityProblems(problems);
    const std::string expected_label =
        "  Status: FAIL - " + ExpectedLabel(type) + "\n";
    EXPECT_NE(std::string::npos, result.find(expected_label))
        << "Expected label: " << expected_label << "\nGot: " << result;
  }
}

TEST(ConnectivityProblemFormatterTest, AllProxyProblemTypeLabels) {
  constexpr GoogleServicesConnectivityProxyProblemType kAllProxyTypes[] = {
      GoogleServicesConnectivityProxyProblemType::kProxyDnsResolutionError,
      GoogleServicesConnectivityProxyProblemType::kProxyConnectionFailure,
  };
  for (auto type : kAllProxyTypes) {
    SCOPED_TRACE(base::ToString(type));
    std::vector<GoogleServicesConnectivityProblemPtr> problems;
    problems.emplace_back(
        MakeProxyConnectionError("host", "proxy:8080", type, "err"));
    const std::string result = FormatConnectivityProblems(problems);
    const std::string expected_label =
        "  Status: FAIL - " + ExpectedLabel(type) + "\n";
    EXPECT_NE(std::string::npos, result.find(expected_label))
        << "Expected label: " << expected_label << "\nGot: " << result;
  }
}

TEST(ConnectivityProblemFormatterTest, ConnectionErrorEmptyHostname) {
  static constexpr char kErrorMessage[] =
      "Failed to execute the connectivity test.";
  static constexpr char kExpected[] =
      "  Status: FAIL - InternalError\n"
      "  Error: Failed to execute the connectivity test.\n";

  std::vector<GoogleServicesConnectivityProblemPtr> problems;
  problems.emplace_back(MakeConnectionError(
      /*hostname=*/"", GoogleServicesConnectivityProblemType::kInternalError,
      kErrorMessage));
  const std::string result = FormatConnectivityProblems(problems);

  // Empty hostname should not produce a leading ":\n" line.
  EXPECT_EQ(std::string::npos, result.find(":\n"));
  EXPECT_EQ(kExpected, result);
}

}  // namespace
}  // namespace ash::diagnostics
