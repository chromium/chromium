// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/https_latency_routine.h"

#include <memory>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/run_loop.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

const base::TimeDelta kNoProblemDelayMs = base::Milliseconds(100);
const base::TimeDelta kHighLatencyDelayMs = base::Milliseconds(550);
const base::TimeDelta kVeryHighLatencyDelayMs = base::Milliseconds(1050);

// The number of hosts the the routine tests for. Based on GetHostnamesToQuery()
// in https_latency_routine.cc.
const int kTotalHosts = 3;
// Represents a fake port number of a fake ip address returned by the
// FakeHostResolver.
const int kFakePortNumber = 1234;
const char kFakeTestProfile[] = "test";

net::IPEndPoint FakeIPAddress() {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), kFakePortNumber);
}

class FakeHostResolver : public network::mojom::HostResolver {
 public:
  struct DnsResult {
    DnsResult(int32_t result,
              net::ResolveErrorInfo resolve_error_info,
              absl::optional<net::AddressList> resolved_addresses,
              absl::optional<net::HostResolverEndpointResults>
                  endpoint_results_with_metadata)
        : result(result),
          resolve_error_info(resolve_error_info),
          resolved_addresses(resolved_addresses),
          endpoint_results_with_metadata(endpoint_results_with_metadata) {}

    int result;
    net::ResolveErrorInfo resolve_error_info;
    absl::optional<net::AddressList> resolved_addresses;
    absl::optional<net::HostResolverEndpointResults>
        endpoint_results_with_metadata;
  };

  FakeHostResolver(mojo::PendingReceiver<network::mojom::HostResolver> receiver,
                   DnsResult* fake_dns_result)
      : receiver_(this, std::move(receiver)),
        fake_dns_result_(fake_dns_result) {}
  ~FakeHostResolver() override {}

  // network::mojom::HostResolver
  void ResolveHost(
      network::mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient>
          pending_response_client) override {
    mojo::Remote<network::mojom::ResolveHostClient> response_client(
        std::move(pending_response_client));
    response_client->OnComplete(
        fake_dns_result_->result, fake_dns_result_->resolve_error_info,
        fake_dns_result_->resolved_addresses,
        fake_dns_result_->endpoint_results_with_metadata);
  }
  void MdnsListen(
      const net::HostPortPair& host,
      net::DnsQueryType query_type,
      mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
      MdnsListenCallback callback) override {
    NOTREACHED();
  }

 private:
  mojo::Receiver<network::mojom::HostResolver> receiver_;
  DnsResult* fake_dns_result_;
};

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  FakeNetworkContext() = default;

  explicit FakeNetworkContext(
      base::circular_deque<FakeHostResolver::DnsResult*> fake_dns_results)
      : fake_dns_results_(std::move(fake_dns_results)) {}

  ~FakeNetworkContext() override {}

  // network::TestNetworkContext:
  void CreateHostResolver(
      const absl::optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) override {
    FakeHostResolver::DnsResult* result = fake_dns_results_.front();
    DCHECK(result);

    fake_dns_results_.pop_front();
    resolver_ = std::make_unique<FakeHostResolver>(std::move(receiver), result);
  }

 private:
  std::unique_ptr<FakeHostResolver> resolver_;
  base::circular_deque<FakeHostResolver::DnsResult*> fake_dns_results_;
};

class FakeTickClock : public base::TickClock {
 public:
  // The |request_delay| fakes the duration of an HTTP request.
  explicit FakeTickClock(
      const base::TimeDelta& request_delay = base::TimeDelta())
      : request_delay_(request_delay) {}
  FakeTickClock(const FakeTickClock&) = delete;
  FakeTickClock& operator=(const FakeTickClock&) = delete;
  ~FakeTickClock() override = default;

  base::TimeTicks NowTicks() const override {
    base::TimeTicks current = current_time_;
    // Advance the current time by |request_delay_| so that each NowTicks()
    // invocation will have this delay. This allows tests to mimic realistic
    // time conditions.
    current_time_ = current_time_ + request_delay_;
    return current;
  }

 private:
  mutable base::TimeTicks current_time_ = base::TimeTicks::Now();
  const base::TimeDelta request_delay_;
};

// Fake implementation of HttpRequestManager used to facilitate testing.
class FakeHttpRequestManager final : public HttpRequestManager {
 public:
  FakeHttpRequestManager() : HttpRequestManager(nullptr) {}
  FakeHttpRequestManager(const FakeHttpRequestManager&) = delete;
  FakeHttpRequestManager& operator=(const FakeHttpRequestManager&) = delete;
  ~FakeHttpRequestManager() override = default;

  // HttpRequestManager:
  void MakeRequest(const GURL& url,
                   const base::TimeDelta& timeout,
                   HttpRequestCallback callback) override {
    std::move(callback).Run(connected_);
  }

  void set_connected(bool connected) { connected_ = connected; }

 private:
  bool connected_ = false;
};

}  // namespace

class HttpsLatencyRoutineTest : public ::testing::Test {
 public:
  HttpsLatencyRoutineTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    session_manager::SessionManager::Get()->SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);
  }

  HttpsLatencyRoutineTest(const HttpsLatencyRoutineTest&) = delete;
  HttpsLatencyRoutineTest& operator=(const HttpsLatencyRoutineTest&) = delete;
  ~HttpsLatencyRoutineTest() override = default;

  void CompareResult(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::HttpsLatencyProblem>& expected_problems,
      mojom::RoutineResultPtr result) {
    EXPECT_EQ(expected_verdict, result->verdict);
    EXPECT_EQ(expected_problems,
              result->problems->get_https_latency_problems());
    run_loop_.Quit();
  }

  void CompareResultNoExpectedLatency(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::HttpsLatencyProblem>& expected_problems,
      mojom::RoutineResultPtr result) {
    EXPECT_TRUE(result->result_value.is_null());
    CompareResult(expected_verdict, expected_problems, std::move(result));
  }

  void CompareResultWithExpectedLatency(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::HttpsLatencyProblem>& expected_problems,
      base::TimeDelta expected_latency,
      mojom::RoutineResultPtr result) {
    ASSERT_FALSE(result->result_value.is_null());
    ASSERT_TRUE(result->result_value->is_https_latency_result_value());
    EXPECT_EQ(result->result_value->get_https_latency_result_value()->latency,
              expected_latency);
    CompareResult(expected_verdict, expected_problems, std::move(result));
  }

 protected:
  void RunRoutineNoExpectedLatency(
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::HttpsLatencyProblem>& expected_problems) {
    https_latency_routine_->RunRoutine(base::BindOnce(
        &HttpsLatencyRoutineTest::CompareResultNoExpectedLatency, weak_ptr(),
        expected_routine_verdict, expected_problems));
    run_loop_.Run();
  }
  void RunRoutineWithExpectedLatency(
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::HttpsLatencyProblem>& expected_problems,
      base::TimeDelta expected_latency) {
    https_latency_routine_->RunRoutine(base::BindOnce(
        &HttpsLatencyRoutineTest::CompareResultWithExpectedLatency, weak_ptr(),
        expected_routine_verdict, expected_problems, expected_latency));
    run_loop_.Run();
  }

  void SetUpRoutine(
      base::circular_deque<FakeHostResolver::DnsResult*> fake_dns_results,
      bool connected,
      const base::TickClock* fake_tick_clock) {
    ASSERT_TRUE(profile_manager_.SetUp());

    // Set up the network context.
    fake_network_context_ =
        std::make_unique<FakeNetworkContext>(std::move(fake_dns_results));
    test_profile_ = profile_manager_.CreateTestingProfile(kFakeTestProfile);

    // Set up routine with fakes.
    https_latency_routine_ = std::make_unique<HttpsLatencyRoutine>();
    https_latency_routine_->set_network_context_getter(base::BindRepeating(
        &HttpsLatencyRoutineTest::GetNetworkContext, base::Unretained(this)));
    https_latency_routine_->set_http_request_manager_getter(
        base::BindRepeating(&HttpsLatencyRoutineTest::GetHttpRequestManager,
                            base::Unretained(this), connected));
    https_latency_routine_->set_tick_clock_for_testing(fake_tick_clock);
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    return fake_network_context_.get();
  }

  std::unique_ptr<HttpRequestManager> GetHttpRequestManager(bool connected) {
    auto http_request_manager = std::make_unique<FakeHttpRequestManager>();
    http_request_manager->set_connected(connected);
    return std::move(http_request_manager);
  }

  base::WeakPtr<HttpsLatencyRoutineTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<FakeNetworkContext> fake_network_context_;
  Profile* test_profile_;  // Unowned
  TestingProfileManager profile_manager_;
  std::unique_ptr<HttpsLatencyRoutine> https_latency_routine_;
  base::WeakPtrFactory<HttpsLatencyRoutineTest> weak_factory_{this};
};

TEST_F(HttpsLatencyRoutineTest, TestFailedDnsResolution) {
  base::circular_deque<FakeHostResolver::DnsResult*> fake_dns_results;
  std::vector<std::unique_ptr<FakeHostResolver::DnsResult>> resolutions;

  // kTotalHosts = 3
  for (int i = 0; i < kTotalHosts; i++) {
    std::unique_ptr<FakeHostResolver::DnsResult> resolution;
    if (i == 2) {
      resolution = std::make_unique<FakeHostResolver::DnsResult>(
          net::ERR_NAME_NOT_RESOLVED,
          net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED), net::AddressList(),
          /*endpoint_results_with_metadata=*/absl::nullopt);
    } else {
      resolution = std::make_unique<FakeHostResolver::DnsResult>(
          net::OK, net::ResolveErrorInfo(net::OK),
          net::AddressList(FakeIPAddress()),
          /*endpoint_results_with_metadata=*/absl::nullopt);
    }
    fake_dns_results.push_back(resolution.get());
    resolutions.emplace_back(std::move(resolution));
  }

  std::unique_ptr<FakeTickClock> fake_tick_clock =
      std::make_unique<FakeTickClock>(kNoProblemDelayMs);

  SetUpRoutine(std::move(fake_dns_results), true, fake_tick_clock.get());
  RunRoutineNoExpectedLatency(
      mojom::RoutineVerdict::kProblem,
      {mojom::HttpsLatencyProblem::kFailedDnsResolutions});
}

TEST_F(HttpsLatencyRoutineTest, TestLowLatency) {
  base::circular_deque<FakeHostResolver::DnsResult*> fake_dns_results;
  std::vector<std::unique_ptr<FakeHostResolver::DnsResult>> resolutions;

  // kTotalHosts = 3
  for (int i = 0; i < kTotalHosts; i++) {
    auto resolution = std::make_unique<FakeHostResolver::DnsResult>(
        net::OK, net::ResolveErrorInfo(net::OK),
        net::AddressList(FakeIPAddress()),
        /*endpoint_results_with_metadata=*/absl::nullopt);
    fake_dns_results.push_back(resolution.get());
    resolutions.emplace_back(std::move(resolution));
  }

  std::unique_ptr<FakeTickClock> fake_tick_clock =
      std::make_unique<FakeTickClock>(kNoProblemDelayMs);

  SetUpRoutine(std::move(fake_dns_results), true, fake_tick_clock.get());
  RunRoutineWithExpectedLatency(mojom::RoutineVerdict::kNoProblem, {},
                                kNoProblemDelayMs);
}

TEST_F(HttpsLatencyRoutineTest, TestFailedHttpRequest) {
  base::circular_deque<FakeHostResolver::DnsResult*> fake_dns_results;
  std::vector<std::unique_ptr<FakeHostResolver::DnsResult>> resolutions;

  // kTotalHosts = 3
  for (int i = 0; i < kTotalHosts; i++) {
    auto resolution = std::make_unique<FakeHostResolver::DnsResult>(
        net::OK, net::ResolveErrorInfo(net::OK),
        net::AddressList(FakeIPAddress()),
        /*endpoint_results_with_metadata=*/absl::nullopt);
    fake_dns_results.push_back(resolution.get());
    resolutions.emplace_back(std::move(resolution));
  }

  std::unique_ptr<FakeTickClock> fake_tick_clock =
      std::make_unique<FakeTickClock>(kNoProblemDelayMs);

  SetUpRoutine(std::move(fake_dns_results), false, fake_tick_clock.get());
  RunRoutineNoExpectedLatency(
      mojom::RoutineVerdict::kProblem,
      {mojom::HttpsLatencyProblem::kFailedHttpsRequests});
}

TEST_F(HttpsLatencyRoutineTest, TestHighLatency) {
  base::circular_deque<FakeHostResolver::DnsResult*> fake_dns_results;
  std::vector<std::unique_ptr<FakeHostResolver::DnsResult>> resolutions;

  // kTotalHosts = 3
  for (int i = 0; i < kTotalHosts; i++) {
    auto resolution = std::make_unique<FakeHostResolver::DnsResult>(
        net::OK, net::ResolveErrorInfo(net::OK),
        net::AddressList(FakeIPAddress()),
        /*endpoint_results_with_metadata=*/absl::nullopt);
    fake_dns_results.push_back(resolution.get());
    resolutions.emplace_back(std::move(resolution));
  }

  std::unique_ptr<FakeTickClock> fake_tick_clock =
      std::make_unique<FakeTickClock>(kHighLatencyDelayMs);

  SetUpRoutine(std::move(fake_dns_results), true, fake_tick_clock.get());
  RunRoutineWithExpectedLatency(mojom::RoutineVerdict::kProblem,
                                {mojom::HttpsLatencyProblem::kHighLatency},
                                kHighLatencyDelayMs);
}

TEST_F(HttpsLatencyRoutineTest, TestVeryHighLatency) {
  base::circular_deque<FakeHostResolver::DnsResult*> fake_dns_results;
  std::vector<std::unique_ptr<FakeHostResolver::DnsResult>> resolutions;

  // kTotalHosts = 3
  for (int i = 0; i < kTotalHosts; i++) {
    auto resolution = std::make_unique<FakeHostResolver::DnsResult>(
        net::OK, net::ResolveErrorInfo(net::OK),
        net::AddressList(FakeIPAddress()),
        /*endpoint_results_with_metadata=*/absl::nullopt);
    fake_dns_results.push_back(resolution.get());
    resolutions.emplace_back(std::move(resolution));
  }

  std::unique_ptr<FakeTickClock> fake_tick_clock =
      std::make_unique<FakeTickClock>(kVeryHighLatencyDelayMs);

  SetUpRoutine(std::move(fake_dns_results), true, fake_tick_clock.get());
  RunRoutineWithExpectedLatency(mojom::RoutineVerdict::kProblem,
                                {mojom::HttpsLatencyProblem::kVeryHighLatency},
                                kVeryHighLatencyDelayMs);
}

}  // namespace network_diagnostics
}  // namespace ash
