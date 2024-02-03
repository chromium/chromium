// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/https_latency_routine.h"

#include <memory>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/net/network_diagnostics/fake_network_context.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::network_diagnostics {

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

  void RunRoutineNoExpectedLatency(
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::HttpsLatencyProblem>& expected_problems) {
    base::test::TestFuture<mojom::RoutineResultPtr> future;
    https_latency_routine_->RunRoutine(future.GetCallback());
    auto result = future.Take();
    EXPECT_TRUE(result->result_value.is_null());
    EXPECT_EQ(expected_routine_verdict, result->verdict);
    EXPECT_EQ(expected_problems,
              result->problems->get_https_latency_problems());
  }

  void RunRoutineWithExpectedLatency(
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::HttpsLatencyProblem>& expected_problems,
      base::TimeDelta expected_latency) {
    base::test::TestFuture<mojom::RoutineResultPtr> future;
    https_latency_routine_->RunRoutine(future.GetCallback());
    auto result = future.Take();
    ASSERT_FALSE(result->result_value.is_null());
    ASSERT_TRUE(result->result_value->is_https_latency_result_value());
    EXPECT_EQ(result->result_value->get_https_latency_result_value()->latency,
              expected_latency);
    EXPECT_EQ(expected_routine_verdict, result->verdict);
    EXPECT_EQ(expected_problems,
              result->problems->get_https_latency_problems());
  }

  void SetUpRoutine(
      base::circular_deque<std::unique_ptr<FakeNetworkContext::DnsResult>>
          fake_dns_results,
      bool connected,
      const base::TickClock* fake_tick_clock) {
    ASSERT_TRUE(profile_manager_.SetUp());

    // Set up the network context.
    fake_network_context_ = std::make_unique<FakeNetworkContext>();
    fake_network_context_->set_fake_dns_results(std::move(fake_dns_results));
    test_profile_ = profile_manager_.CreateTestingProfile(kFakeTestProfile);

    // Set up routine with fakes.
    https_latency_routine_ = std::make_unique<HttpsLatencyRoutine>(
        mojom::RoutineCallSource::kDiagnosticsUI);
    https_latency_routine_->set_network_context_getter(base::BindRepeating(
        &HttpsLatencyRoutineTest::GetNetworkContext, base::Unretained(this)));
    https_latency_routine_->set_http_request_manager_getter(
        base::BindRepeating(&HttpsLatencyRoutineTest::GetHttpRequestManager,
                            base::Unretained(this), connected));
    https_latency_routine_->set_tick_clock_for_testing(fake_tick_clock);
  }

 protected:
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
  session_manager::SessionManager session_manager_;
  std::unique_ptr<FakeNetworkContext> fake_network_context_;
  raw_ptr<Profile, DanglingUntriaged> test_profile_;  // Unowned
  TestingProfileManager profile_manager_;
  std::unique_ptr<HttpsLatencyRoutine> https_latency_routine_;
  base::WeakPtrFactory<HttpsLatencyRoutineTest> weak_factory_{this};
};

TEST_F(HttpsLatencyRoutineTest, TestFailedDnsResolution) {
  base::circular_deque<std::unique_ptr<FakeNetworkContext::DnsResult>>
      fake_dns_results;

  // kTotalHosts = 3
  for (int i = 0; i < kTotalHosts; i++) {
    if (i == 2) {
      fake_dns_results.emplace_back(
          std::make_unique<FakeNetworkContext::DnsResult>(
              net::ERR_NAME_NOT_RESOLVED,
              net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
              /*resolved_addresses=*/std::nullopt,
              /*endpoint_results_with_metadata=*/std::nullopt));
    } else {
      fake_dns_results.emplace_back(
          std::make_unique<FakeNetworkContext::DnsResult>(
              net::OK, net::ResolveErrorInfo(net::OK),
              net::AddressList(FakeIPAddress()),
              /*endpoint_results_with_metadata=*/std::nullopt));
    }
  }

  std::unique_ptr<FakeTickClock> fake_tick_clock =
      std::make_unique<FakeTickClock>(kNoProblemDelayMs);

  SetUpRoutine(std::move(fake_dns_results), true, fake_tick_clock.get());
  RunRoutineNoExpectedLatency(
      mojom::RoutineVerdict::kProblem,
      {mojom::HttpsLatencyProblem::kFailedDnsResolutions});
}

TEST_F(HttpsLatencyRoutineTest, TestLowLatency) {
  base::circular_deque<std::unique_ptr<FakeNetworkContext::DnsResult>>
      fake_dns_results;

  // kTotalHosts = 3
  for (int i = 0; i < kTotalHosts; i++) {
    fake_dns_results.emplace_back(
        std::make_unique<FakeNetworkContext::DnsResult>(
            net::OK, net::ResolveErrorInfo(net::OK),
            net::AddressList(FakeIPAddress()),
            /*endpoint_results_with_metadata=*/std::nullopt));
  }

  std::unique_ptr<FakeTickClock> fake_tick_clock =
      std::make_unique<FakeTickClock>(kNoProblemDelayMs);

  SetUpRoutine(std::move(fake_dns_results), true, fake_tick_clock.get());
  RunRoutineWithExpectedLatency(mojom::RoutineVerdict::kNoProblem, {},
                                kNoProblemDelayMs);
}

TEST_F(HttpsLatencyRoutineTest, TestFailedHttpRequest) {
  base::circular_deque<std::unique_ptr<FakeNetworkContext::DnsResult>>
      fake_dns_results;

  // kTotalHosts = 3
  for (int i = 0; i < kTotalHosts; i++) {
    fake_dns_results.emplace_back(
        std::make_unique<FakeNetworkContext::DnsResult>(
            net::OK, net::ResolveErrorInfo(net::OK),
            net::AddressList(FakeIPAddress()),
            /*endpoint_results_with_metadata=*/std::nullopt));
  }

  std::unique_ptr<FakeTickClock> fake_tick_clock =
      std::make_unique<FakeTickClock>(kNoProblemDelayMs);

  SetUpRoutine(std::move(fake_dns_results), false, fake_tick_clock.get());
  RunRoutineNoExpectedLatency(
      mojom::RoutineVerdict::kProblem,
      {mojom::HttpsLatencyProblem::kFailedHttpsRequests});
}

TEST_F(HttpsLatencyRoutineTest, TestHighLatency) {
  base::circular_deque<std::unique_ptr<FakeNetworkContext::DnsResult>>
      fake_dns_results;

  // kTotalHosts = 3
  for (int i = 0; i < kTotalHosts; i++) {
    fake_dns_results.emplace_back(
        std::make_unique<FakeNetworkContext::DnsResult>(
            net::OK, net::ResolveErrorInfo(net::OK),
            net::AddressList(FakeIPAddress()),
            /*endpoint_results_with_metadata=*/std::nullopt));
  }

  std::unique_ptr<FakeTickClock> fake_tick_clock =
      std::make_unique<FakeTickClock>(kHighLatencyDelayMs);

  SetUpRoutine(std::move(fake_dns_results), true, fake_tick_clock.get());
  RunRoutineWithExpectedLatency(mojom::RoutineVerdict::kProblem,
                                {mojom::HttpsLatencyProblem::kHighLatency},
                                kHighLatencyDelayMs);
}

TEST_F(HttpsLatencyRoutineTest, TestVeryHighLatency) {
  base::circular_deque<std::unique_ptr<FakeNetworkContext::DnsResult>>
      fake_dns_results;

  // kTotalHosts = 3
  for (int i = 0; i < kTotalHosts; i++) {
    fake_dns_results.emplace_back(
        std::make_unique<FakeNetworkContext::DnsResult>(
            net::OK, net::ResolveErrorInfo(net::OK),
            net::AddressList(FakeIPAddress()),
            /*endpoint_results_with_metadata=*/std::nullopt));
  }

  std::unique_ptr<FakeTickClock> fake_tick_clock =
      std::make_unique<FakeTickClock>(kVeryHighLatencyDelayMs);

  SetUpRoutine(std::move(fake_dns_results), true, fake_tick_clock.get());
  RunRoutineWithExpectedLatency(mojom::RoutineVerdict::kProblem,
                                {mojom::HttpsLatencyProblem::kVeryHighLatency},
                                kVeryHighLatencyDelayMs);
}

}  // namespace ash::network_diagnostics
