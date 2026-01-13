// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/google_services_connectivity_routine.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/net/network_diagnostics/google_services_connectivity_routine_util.h"
#include "chrome/browser/ash/net/network_diagnostics/hosts_connectivity_diagnostics.pb.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

std::vector<uint8_t> SerializeProto(
    const hosts_connectivity_diagnostics::TestConnectivityResponse& response) {
  std::vector<uint8_t> result(response.ByteSizeLong());
  response.SerializeToArray(result.data(), result.size());
  return result;
}

// Converts a protobuf entry to an expected mojom problem union.
// Returns nullopt for SUCCESS/UNSPECIFIED entries.
std::optional<mojom::GoogleServicesConnectivityProblemPtr>
ProtoEntryToExpectedProblem(
    const hosts_connectivity_diagnostics::ConnectivityResultEntry& entry) {
  const auto result_code = entry.result_code();

  if (IsSuccessfulOrUnspecifiedResult(result_code)) {
    return std::nullopt;
  }

  std::optional<std::string> resolution_message;
  if (!entry.resolution_message().empty()) {
    resolution_message = entry.resolution_message();
  }

  auto error_details = mojom::GoogleServicesConnectivityErrorDetails::New(
      entry.error_message(), std::move(resolution_message));

  if (IsNoValidProxyError(result_code)) {
    // NO_VALID_PROXY: no timestamps, hostname/proxy are optional.
    std::optional<std::string> hostname;
    if (!entry.hostname().empty()) {
      hostname = entry.hostname();
    }
    std::optional<std::string> proxy;
    if (!entry.proxy().empty()) {
      proxy = entry.proxy();
    }
    return mojom::GoogleServicesConnectivityProblem::NewNoValidProxyError(
        mojom::GoogleServicesConnectivityNoValidProxyError::New(
            std::move(hostname), std::move(proxy), std::move(error_details)));
  }

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
    // PROXY_DNS_RESOLUTION_ERROR or PROXY_CONNECTION_FAILURE:
    // proxy is always present.
    DCHECK(!entry.proxy().empty());
    return mojom::GoogleServicesConnectivityProblem::NewProxyConnectionError(
        mojom::GoogleServicesConnectivityProxyConnectionError::New(
            ToProxyProblemType(result_code), entry.proxy(),
            std::move(connection_info)));
  }

  // All other connection errors: proxy is optional.
  std::optional<std::string> proxy;
  if (!entry.proxy().empty()) {
    proxy = entry.proxy();
  }
  return mojom::GoogleServicesConnectivityProblem::NewConnectionError(
      mojom::GoogleServicesConnectivityConnectionError::New(
          ToConnectivityProblemType(result_code), std::move(proxy),
          std::move(connection_info)));
}

hosts_connectivity_diagnostics::ConnectivityResultEntry
CreateConnectivityResultEntry(
    hosts_connectivity_diagnostics::ConnectivityResultCode result_code,
    std::optional<std::string> hostname,
    std::optional<std::string> proxy,
    std::optional<std::string> error_message,
    std::optional<std::string> resolution_message,
    bool set_timestamps) {
  hosts_connectivity_diagnostics::ConnectivityResultEntry entry;
  entry.set_result_code(result_code);
  if (hostname.has_value()) {
    entry.set_hostname(*hostname);
  }
  if (proxy.has_value()) {
    entry.set_proxy(*proxy);
  }
  if (error_message.has_value()) {
    entry.set_error_message(*error_message);
  }
  if (resolution_message.has_value()) {
    entry.set_resolution_message(*resolution_message);
  }
  if (set_timestamps) {
    entry.set_timestamp_start(123456789);
    entry.set_timestamp_end(123456999);
  }
  return entry;
}

hosts_connectivity_diagnostics::ConnectivityResultEntry CreateSuccessEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::SUCCESS,
      "clients1.google.com", /*proxy=*/std::nullopt,
      /*error_message=*/std::nullopt,
      /*resolution_message=*/std::nullopt, /*set_timestamps=*/true);
}

hosts_connectivity_diagnostics::ConnectivityResultEntry
CreateDnsResolutionErrorEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::
          DNS_RESOLUTION_ERROR,
      "clients1.google.com", "test_proxy.com",
      "DNS resolution failed: NXDOMAIN",
      /*resolution_message=*/std::nullopt, /*set_timestamps=*/true);
}

hosts_connectivity_diagnostics::ConnectivityResultEntry
CreateConnectionFailureEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::
          CONNECTION_FAILURE,
      "clients3.google.com", "192.168.1.1:1212", "Connection refused by host",
      /*resolution_message=*/std::nullopt, /*set_timestamps=*/true);
}

hosts_connectivity_diagnostics::ConnectivityResultEntry
CreateConnectionTimeoutEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::
          CONNECTION_TIMEOUT,
      "clients4.google.com", /*proxy=*/std::nullopt,
      "Request timed out after 10 seconds", /*resolution_message=*/std::nullopt,
      /*set_timestamps=*/true);
}

hosts_connectivity_diagnostics::ConnectivityResultEntry
CreateSslConnectionErrorEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::
          SSL_CONNECTION_ERROR,
      "clients5.google.com", /*proxy=*/std::nullopt, "SSL handshake failed",
      /*resolution_message=*/std::nullopt, /*set_timestamps=*/true);
}

hosts_connectivity_diagnostics::ConnectivityResultEntry
CreatePeerCertificateErrorEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::
          PEER_CERTIFICATE_ERROR,
      "clients6.google.com", /*proxy=*/std::nullopt, "Certificate expired",
      /*resolution_message=*/std::nullopt, /*set_timestamps=*/true);
}

hosts_connectivity_diagnostics::ConnectivityResultEntry
CreateInternalErrorEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::INTERNAL_ERROR,
      "clients1.google.com", /*proxy=*/std::nullopt,
      "Unexpected error occurred",
      /*resolution_message=*/std::nullopt, /*set_timestamps=*/true);
}

// Per protobuf spec: hostname=always, proxy=always.
hosts_connectivity_diagnostics::ConnectivityResultEntry
CreateProxyDnsResolutionErrorEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::
          PROXY_DNS_RESOLUTION_ERROR,
      "clients1.google.com", "proxy.example.com:8080",
      "Unable to resolve proxy DNS", "Check proxy configuration", true);
}

// Per protobuf spec: hostname=always, proxy=always.
hosts_connectivity_diagnostics::ConnectivityResultEntry
CreateProxyConnectionFailureEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::
          PROXY_CONNECTION_FAILURE,
      "clients2.google.com", "192.168.1.1:3128", "Connection to proxy failed",
      /*resolution_message=*/std::nullopt, /*set_timestamps=*/true);
}

// Per protobuf spec: timestamps=never.
hosts_connectivity_diagnostics::ConnectivityResultEntry
CreateNoValidHostnameEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::NO_VALID_HOSTNAME,
      /*hostname=*/std::nullopt, /*proxy=*/std::nullopt,
      "No valid hostnames provided", /*resolution_message=*/std::nullopt,
      /*set_timestamps=*/false);
}

// Per protobuf spec: timestamps=never.
hosts_connectivity_diagnostics::ConnectivityResultEntry
CreateNoValidProxyEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::NO_VALID_PROXY,
      /*hostname=*/std::nullopt, "invalid-proxy-url",
      "Invalid proxy URL provided",
      /*resolution_message=*/std::nullopt, /*set_timestamps=*/false);
}

hosts_connectivity_diagnostics::ConnectivityResultEntry
CreateUnknownErrorEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::UNKNOWN_ERROR,
      "clients1.google.com", /*proxy=*/std::nullopt, "Unknown error occurred",
      /*resolution_message=*/std::nullopt, /*set_timestamps=*/true);
}

hosts_connectivity_diagnostics::ConnectivityResultEntry CreateHttpErrorEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::HTTP_ERROR,
      "clients2.google.com", /*proxy=*/std::nullopt,
      "HTTP 503 Service Unavailable",
      /*resolution_message=*/std::nullopt, /*set_timestamps=*/true);
}

hosts_connectivity_diagnostics::ConnectivityResultEntry
CreateNoNetworkErrorEntry() {
  return CreateConnectivityResultEntry(
      hosts_connectivity_diagnostics::ConnectivityResultCode::NO_NETWORK_ERROR,
      "clients3.google.com", /*proxy=*/std::nullopt,
      "Network disconnected during test", /*resolution_message=*/std::nullopt,
      /*set_timestamps=*/true);
}

hosts_connectivity_diagnostics::TestConnectivityResponse
CreateSuccessfulConnectivityResponse() {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  *response.add_connectivity_results() = CreateSuccessEntry();
  return response;
}

class FakeGoogleServicesDebugDaemonClient : public ash::FakeDebugDaemonClient {
 public:
  FakeGoogleServicesDebugDaemonClient() = default;

  explicit FakeGoogleServicesDebugDaemonClient(
      const std::vector<uint8_t>& connectivity_response)
      : connectivity_response_(connectivity_response) {}

  FakeGoogleServicesDebugDaemonClient(
      const FakeGoogleServicesDebugDaemonClient&) = delete;
  FakeGoogleServicesDebugDaemonClient& operator=(
      const FakeGoogleServicesDebugDaemonClient&) = delete;

  ~FakeGoogleServicesDebugDaemonClient() override = default;

  void TestHostsConnectivity(
      const std::vector<std::string>& hosts,
      const base::flat_map<std::string, std::string>& options,
      TestHostsConnectivityCallback callback) override {
    std::move(callback).Run(connectivity_response_);
  }

 private:
  std::vector<uint8_t> connectivity_response_;
};

}  // namespace

class GoogleServicesConnectivityRoutineTest : public testing::Test {
 public:
  GoogleServicesConnectivityRoutineTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kGoogleServicesConnectivityRoutine);
  }
  GoogleServicesConnectivityRoutineTest(
      const GoogleServicesConnectivityRoutineTest&) = delete;
  GoogleServicesConnectivityRoutineTest& operator=(
      const GoogleServicesConnectivityRoutineTest&) = delete;
  ~GoogleServicesConnectivityRoutineTest() override = default;

  void SetUpRoutineWithSerializedResponse(
      const std::vector<uint8_t>& serialized_response) {
    debug_daemon_client_ =
        std::make_unique<FakeGoogleServicesDebugDaemonClient>(
            serialized_response);
    google_services_connectivity_routine_ =
        std::make_unique<GoogleServicesConnectivityRoutine>(
            mojom::RoutineCallSource::kDiagnosticsUI,
            debug_daemon_client_.get());
  }

  void SetUpRoutine(
      const hosts_connectivity_diagnostics::TestConnectivityResponse&
          response) {
    SetUpRoutineWithSerializedResponse(SerializeProto(response));
  }

  void RunAndExpect(
      const hosts_connectivity_diagnostics::TestConnectivityResponse& expected,
      mojom::RoutineVerdict expected_verdict) {
    base::test::TestFuture<mojom::RoutineResultPtr> future;
    google_services_connectivity_routine_->RunRoutine(future.GetCallback());
    const auto& result = future.Get();
    EXPECT_EQ(expected_verdict, result->verdict);

    const auto& actual_problems =
        result->problems->get_google_services_connectivity_problems();

    std::vector<mojom::GoogleServicesConnectivityProblemPtr> expected_problems;
    for (const auto& entry : expected.connectivity_results()) {
      auto problem = ProtoEntryToExpectedProblem(entry);
      if (problem.has_value()) {
        expected_problems.push_back(std::move(problem.value()));
      }
    }

    EXPECT_EQ(actual_problems, expected_problems);
  }

  void RunAndExpectInternalError(const std::string& expected_error_message) {
    base::test::TestFuture<mojom::RoutineResultPtr> future;
    google_services_connectivity_routine_->RunRoutine(future.GetCallback());
    const auto& result = future.Get();
    EXPECT_EQ(mojom::RoutineVerdict::kProblem, result->verdict);

    const auto& problems =
        result->problems->get_google_services_connectivity_problems();
    ASSERT_EQ(1u, problems.size());
    ASSERT_TRUE(problems[0]->is_connection_error());
    const auto& conn_error = problems[0]->get_connection_error();
    EXPECT_EQ(mojom::GoogleServicesConnectivityProblemType::kInternalError,
              conn_error->problem_type);
    EXPECT_FALSE(conn_error->proxy.has_value());
    EXPECT_TRUE(conn_error->connection_info->hostname.empty());
    EXPECT_FALSE(
        conn_error->connection_info->error_details->error_message.empty());
    EXPECT_EQ(expected_error_message,
              conn_error->connection_info->error_details->error_message);
  }

  GoogleServicesConnectivityRoutine* google_services_connectivity_routine() {
    return google_services_connectivity_routine_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeGoogleServicesDebugDaemonClient> debug_daemon_client_;
  std::unique_ptr<GoogleServicesConnectivityRoutine>
      google_services_connectivity_routine_;
};

TEST_F(GoogleServicesConnectivityRoutineTest, Type) {
  auto response = CreateSuccessfulConnectivityResponse();
  SetUpRoutine(response);
  EXPECT_EQ(google_services_connectivity_routine()->Type(),
            mojom::RoutineType::kGoogleServicesConnectivity);
}

TEST_F(GoogleServicesConnectivityRoutineTest, NoProblem) {
  auto response = CreateSuccessfulConnectivityResponse();
  SetUpRoutine(response);
  RunAndExpect(response, mojom::RoutineVerdict::kNoProblem);
}

TEST_F(GoogleServicesConnectivityRoutineTest,
       TestAllErrorTypesMappedCorrectly) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  *response.add_connectivity_results() = CreateDnsResolutionErrorEntry();
  *response.add_connectivity_results() = CreateConnectionFailureEntry();
  *response.add_connectivity_results() = CreateConnectionTimeoutEntry();
  *response.add_connectivity_results() = CreateSslConnectionErrorEntry();
  *response.add_connectivity_results() = CreatePeerCertificateErrorEntry();
  *response.add_connectivity_results() = CreateInternalErrorEntry();
  *response.add_connectivity_results() = CreateProxyDnsResolutionErrorEntry();
  *response.add_connectivity_results() = CreateProxyConnectionFailureEntry();
  *response.add_connectivity_results() = CreateNoValidHostnameEntry();
  *response.add_connectivity_results() = CreateNoValidProxyEntry();
  *response.add_connectivity_results() = CreateUnknownErrorEntry();
  *response.add_connectivity_results() = CreateHttpErrorEntry();
  *response.add_connectivity_results() = CreateNoNetworkErrorEntry();
  SetUpRoutine(response);
  RunAndExpect(response, mojom::RoutineVerdict::kProblem);
}

TEST_F(GoogleServicesConnectivityRoutineTest,
       TestSuccessAndUnspecifiedFiltering) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  *response.add_connectivity_results() = CreateDnsResolutionErrorEntry();
  *response.add_connectivity_results() = CreateSuccessEntry();
  *response.add_connectivity_results() = CreateSuccessEntry();
  *response.add_connectivity_results() = CreateConnectionTimeoutEntry();
  *response.add_connectivity_results() =
      hosts_connectivity_diagnostics::ConnectivityResultEntry();
  SetUpRoutine(response);
  RunAndExpect(response, mojom::RoutineVerdict::kProblem);
}

TEST_F(GoogleServicesConnectivityRoutineTest,
       CanRunReturnsTrueWhenFeatureEnabled) {
  auto response = CreateSuccessfulConnectivityResponse();
  SetUpRoutine(response);
  EXPECT_TRUE(google_services_connectivity_routine()->CanRun());
}

TEST_F(GoogleServicesConnectivityRoutineTest, TestAllSuccessEntries) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  *response.add_connectivity_results() = CreateSuccessEntry();
  *response.add_connectivity_results() = CreateSuccessEntry();
  *response.add_connectivity_results() = CreateSuccessEntry();
  SetUpRoutine(response);
  RunAndExpect(response, mojom::RoutineVerdict::kNoProblem);
}

TEST_F(GoogleServicesConnectivityRoutineTest, TestInvalidProtobufResponse) {
  SetUpRoutineWithSerializedResponse({0xFF, 0xFF, 0xFF, 0xFF});
  RunAndExpectInternalError(kGoogleServicesConnectivityInvalidProtobufError);
}

TEST_F(GoogleServicesConnectivityRoutineTest, TestEmptyResponse) {
  SetUpRoutineWithSerializedResponse({});
  RunAndExpectInternalError(kGoogleServicesConnectivityFailedToExecuteError);
}

// Malformed entries are skipped per proto spec field requirements.
TEST_F(GoogleServicesConnectivityRoutineTest,
       TestMalformedProxyErrorEmptyProxy) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  auto& entry = *response.add_connectivity_results() =
      CreateProxyDnsResolutionErrorEntry();
  entry.clear_proxy();
  SetUpRoutine(response);
  RunAndExpect({}, mojom::RoutineVerdict::kNoProblem);
}

TEST_F(GoogleServicesConnectivityRoutineTest,
       TestMalformedConnectionErrorEmptyHostname) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  auto& entry = *response.add_connectivity_results() =
      CreateDnsResolutionErrorEntry();
  entry.clear_hostname();
  SetUpRoutine(response);
  RunAndExpect({}, mojom::RoutineVerdict::kNoProblem);
}

TEST_F(GoogleServicesConnectivityRoutineTest,
       TestMalformedProxyErrorEmptyHostname) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  auto& entry = *response.add_connectivity_results() =
      CreateProxyConnectionFailureEntry();
  entry.clear_hostname();
  SetUpRoutine(response);
  RunAndExpect({}, mojom::RoutineVerdict::kNoProblem);
}

TEST_F(GoogleServicesConnectivityRoutineTest,
       TestMalformedEntrySkippedButValidEntryKept) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  auto& malformed = *response.add_connectivity_results() =
      CreateDnsResolutionErrorEntry();
  malformed.clear_hostname();
  *response.add_connectivity_results() = CreateConnectionTimeoutEntry();
  SetUpRoutine(response);

  // Only valid entry remains after malformed is skipped.
  hosts_connectivity_diagnostics::TestConnectivityResponse expected;
  *expected.add_connectivity_results() = CreateConnectionTimeoutEntry();
  RunAndExpect(expected, mojom::RoutineVerdict::kProblem);
}

TEST_F(GoogleServicesConnectivityRoutineTest,
       TestConnectionErrorMissingTimestampsStillProcessed) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  auto& entry = *response.add_connectivity_results() =
      CreateConnectionFailureEntry();
  entry.clear_timestamp_start();
  entry.clear_timestamp_end();
  SetUpRoutine(response);
  RunAndExpect(response, mojom::RoutineVerdict::kProblem);
}

TEST_F(GoogleServicesConnectivityRoutineTest,
       TestConnectionErrorEmptyErrorMessage) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  auto& entry = *response.add_connectivity_results() =
      CreateConnectionFailureEntry();
  entry.clear_error_message();
  SetUpRoutine(response);
  RunAndExpect({}, mojom::RoutineVerdict::kNoProblem);
}

TEST_F(GoogleServicesConnectivityRoutineTest,
       TestNoValidProxyEmptyErrorMessage) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  auto& entry = *response.add_connectivity_results() =
      CreateNoValidProxyEntry();
  entry.clear_error_message();
  SetUpRoutine(response);
  RunAndExpect({}, mojom::RoutineVerdict::kNoProblem);
}

TEST_F(GoogleServicesConnectivityRoutineTest,
       TestUnknownResultCodeMapsToInternalError) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  auto& entry = *response.add_connectivity_results() =
      CreateConnectionFailureEntry();
  entry.set_result_code(
      static_cast<hosts_connectivity_diagnostics::ConnectivityResultCode>(999));

  SetUpRoutine(response);
  base::test::TestFuture<mojom::RoutineResultPtr> future;
  google_services_connectivity_routine()->RunRoutine(future.GetCallback());
  const auto& result = future.Get();
  EXPECT_EQ(mojom::RoutineVerdict::kProblem, result->verdict);
  const auto& problems =
      result->problems->get_google_services_connectivity_problems();
  ASSERT_EQ(1u, problems.size());
  ASSERT_TRUE(problems[0]->is_connection_error());
  EXPECT_EQ(mojom::GoogleServicesConnectivityProblemType::kInternalError,
            problems[0]->get_connection_error()->problem_type);
}

TEST_F(GoogleServicesConnectivityRoutineTest,
       TestResolutionMessagePassedThrough) {
  hosts_connectivity_diagnostics::TestConnectivityResponse response;
  auto& entry = *response.add_connectivity_results() =
      CreateDnsResolutionErrorEntry();
  entry.set_resolution_message("Check your DNS settings or try 8.8.8.8");
  SetUpRoutine(response);
  RunAndExpect(response, mojom::RoutineVerdict::kProblem);
}

// Test fixture with the feature flag disabled.
class GoogleServicesConnectivityRoutineDisabledTest : public testing::Test {
 public:
  GoogleServicesConnectivityRoutineDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kGoogleServicesConnectivityRoutine);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(GoogleServicesConnectivityRoutineDisabledTest,
       CanNotRunFeatureDisabled) {
  FakeGoogleServicesDebugDaemonClient debug_daemon_client;
  GoogleServicesConnectivityRoutine routine(
      mojom::RoutineCallSource::kDiagnosticsUI, &debug_daemon_client);
  ASSERT_FALSE(routine.CanRun());

  base::test::TestFuture<mojom::RoutineResultPtr> future;
  routine.RunRoutine(future.GetCallback());
  const auto& result = future.Get();
  EXPECT_EQ(result->verdict, mojom::RoutineVerdict::kNotRun);
  EXPECT_FALSE(result->timestamp.is_null());
}

}  // namespace ash::network_diagnostics
