// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/https_firewall_routine.h"

#include <memory>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/net/network_diagnostics/fake_network_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

// The number of hosts the the routine tries to open socket connections to (if
// DNS resolution is successful). Value equals the number of random hosts
// + fixed hosts queried by HttpsFirewallRoutine.
const int kTotalHosts = 9;

// Test implementation of TlsProber.
class TestTlsProber final : public TlsProber {
 public:
  TestTlsProber(TlsProber::TlsProbeCompleteCallback callback,
                int result,
                TlsProber::ProbeExitEnum probe_exit_enum) {
    // Post an asynchronus task simulating a completed probe. This mimics the
    // behavior of the production TlsProber constructor since the TestTlsProber
    // instance will be complete before FinishProbe is invoked. In the
    // production TlsProber, the constructor completes before DNS host
    // resolution is invoked.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&TestTlsProber::FinishProbe, weak_factory_.GetWeakPtr(),
                       std::move(callback), result, probe_exit_enum));
  }

  TestTlsProber(const TestTlsProber&) = delete;
  TestTlsProber& operator=(const TestTlsProber&) = delete;
  ~TestTlsProber() override = default;

 private:
  void FinishProbe(TlsProber::TlsProbeCompleteCallback callback,
                   int result,
                   TlsProber::ProbeExitEnum probe_exit_enum) {
    std::move(callback).Run(result, probe_exit_enum);
  }

  base::WeakPtrFactory<TestTlsProber> weak_factory_{this};
};

}  // namespace

class HttpsFirewallRoutineTest : public ::testing::Test {
 public:
  struct TlsProberReturnValue {
    net::Error result;
    TlsProber::ProbeExitEnum probe_exit_enum;
  };

  HttpsFirewallRoutineTest() = default;
  HttpsFirewallRoutineTest(const HttpsFirewallRoutineTest&) = delete;
  HttpsFirewallRoutineTest& operator=(const HttpsFirewallRoutineTest&) = delete;

  void RunRoutine(
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::HttpsFirewallProblem>& expected_problems) {
    https_firewall_routine_->RunRoutine(base::BindOnce(
        &HttpsFirewallRoutineTest::CompareResult, weak_factory_.GetWeakPtr(),
        expected_routine_verdict, expected_problems));
    run_loop_.Run();
  }

  void CompareResult(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::HttpsFirewallProblem>& expected_problems,
      mojom::RoutineResultPtr result) {
    DCHECK(run_loop_.running());
    EXPECT_EQ(expected_verdict, result->verdict);
    EXPECT_EQ(expected_problems,
              result->problems->get_https_firewall_problems());
    run_loop_.Quit();
  }

  void SetUpRoutine(
      base::circular_deque<TlsProberReturnValue> fake_probe_results) {
    fake_probe_results_ = std::move(fake_probe_results);
    https_firewall_routine_ = std::make_unique<HttpsFirewallRoutine>(
        mojom::RoutineCallSource::kDiagnosticsUI);
    https_firewall_routine_->set_tls_prober_getter_callback_for_testing(
        base::BindRepeating(
            &HttpsFirewallRoutineTest::CreateAndExecuteTlsProber,
            base::Unretained(this)));
  }

  // Sets up required properties (via fakes) and runs the test.
  //
  // Parameters:
  // |fake_probe_results|: Represents the results of TLS probes.
  // |expected_routine_verdict|: Represents the expected verdict
  // reported by this test.
  // |expected_problems|: Represents the expected problem
  // reported by this test.
  void SetUpAndRunRoutine(
      base::circular_deque<TlsProberReturnValue> fake_probe_results,
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::HttpsFirewallProblem>& expected_problems) {
    SetUpRoutine(std::move(fake_probe_results));
    RunRoutine(expected_routine_verdict, expected_problems);
  }

  std::unique_ptr<TlsProber> CreateAndExecuteTlsProber(
      network::NetworkContextGetter network_context_getter,
      net::HostPortPair host_port_pair,
      bool negotiate_tls,
      TlsProber::TlsProbeCompleteCallback callback) {
    DCHECK(fake_probe_results_.size() > 0);

    auto value = fake_probe_results_.front();
    fake_probe_results_.pop_front();
    auto test_tls_prober = std::make_unique<TestTlsProber>(
        std::move(callback), value.result, value.probe_exit_enum);
    return std::move(test_tls_prober);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  base::circular_deque<TlsProberReturnValue> fake_probe_results_;
  std::unique_ptr<HttpsFirewallRoutine> https_firewall_routine_;
  base::WeakPtrFactory<HttpsFirewallRoutineTest> weak_factory_{this};
};

TEST_F(HttpsFirewallRoutineTest, TestHighDnsResolutionFailuresRate) {
  base::circular_deque<TlsProberReturnValue> fake_probe_results;
  // kTotalHosts = 9
  for (int i = 0; i < kTotalHosts; i++) {
    if (i < 2) {
      fake_probe_results.push_back(TlsProberReturnValue{
          net::ERR_NAME_NOT_RESOLVED, TlsProber::ProbeExitEnum::kDnsFailure});
    } else {
      // Having seven successful resolutions out of nine puts us below the
      // threshold needed to attempt TLS probes.
      fake_probe_results.push_back(
          TlsProberReturnValue{net::OK, TlsProber::ProbeExitEnum::kSuccess});
    }
  }
  SetUpAndRunRoutine(
      std::move(fake_probe_results), mojom::RoutineVerdict::kProblem,
      {mojom::HttpsFirewallProblem::kHighDnsResolutionFailureRate});
}

// Edge case for tls_probe_failure_rate calculation.
TEST_F(HttpsFirewallRoutineTest, TestNoDnsResolutionSuccess) {
  base::circular_deque<TlsProberReturnValue> fake_probe_results;
  // kTotalHosts = 9
  for (int i = 0; i < kTotalHosts; i++) {
    fake_probe_results.push_back(TlsProberReturnValue{
        net::ERR_NAME_NOT_RESOLVED, TlsProber::ProbeExitEnum::kDnsFailure});
  }
  SetUpAndRunRoutine(
      std::move(fake_probe_results), mojom::RoutineVerdict::kProblem,
      {mojom::HttpsFirewallProblem::kHighDnsResolutionFailureRate});
}

TEST_F(HttpsFirewallRoutineTest, TestFirewallDetection) {
  base::circular_deque<TlsProberReturnValue> fake_probe_results;
  // kTotalHosts = 9
  for (int i = 0; i < kTotalHosts; i++) {
    fake_probe_results.push_back(TlsProberReturnValue{
        net::ERR_FAILED, TlsProber::ProbeExitEnum::kTlsUpgradeFailure});
  }
  SetUpAndRunRoutine(std::move(fake_probe_results),
                     mojom::RoutineVerdict::kProblem,
                     {mojom::HttpsFirewallProblem::kFirewallDetected});
}

TEST_F(HttpsFirewallRoutineTest, TestPotentialFirewallDetection) {
  base::circular_deque<TlsProberReturnValue> fake_probe_results;
  // kTotalHosts = 9
  for (int i = 0; i < kTotalHosts; i++) {
    if (i < 5) {
      fake_probe_results.push_back(
          TlsProberReturnValue{net::OK, TlsProber::ProbeExitEnum::kSuccess});
    } else {
      // Having five connection failures and four successful connections signals
      // a potential firewall.
      fake_probe_results.push_back(TlsProberReturnValue{
          net::ERR_FAILED, TlsProber::ProbeExitEnum::kTcpConnectionFailure});
    }
  }
  SetUpAndRunRoutine(std::move(fake_probe_results),
                     mojom::RoutineVerdict::kProblem,
                     {mojom::HttpsFirewallProblem::kPotentialFirewall});
}

TEST_F(HttpsFirewallRoutineTest, TestNoFirewallIssues) {
  base::circular_deque<TlsProberReturnValue> fake_probe_results;
  // kTotalHosts = 9
  for (int i = 0; i < kTotalHosts; i++) {
    if (i < 8) {
      fake_probe_results.push_back(
          TlsProberReturnValue{net::OK, TlsProber::ProbeExitEnum::kSuccess});
    } else {
      // Having one connection failure and eight successful connections puts us
      // above the required threshold.
      fake_probe_results.push_back(TlsProberReturnValue{
          net::ERR_FAILED, TlsProber::ProbeExitEnum::kMojoDisconnectFailure});
    }
  }
  SetUpAndRunRoutine(std::move(fake_probe_results),
                     mojom::RoutineVerdict::kNoProblem, {});
}

TEST_F(HttpsFirewallRoutineTest, TestContinousRetries) {
  base::circular_deque<TlsProberReturnValue> fake_probe_results;
  // kTotalHosts = 9
  for (int i = 0; i < kTotalHosts; i++) {
    if (i < 8) {
      fake_probe_results.push_back(
          TlsProberReturnValue{net::OK, TlsProber::ProbeExitEnum::kSuccess});
    } else {
      // Having one socket that continuously retries until failure and eight
      // sockets that make successful connections puts us above the required
      // threshold.
      for (int j = 0; j < kTotalNumRetries + 1; j++) {
        fake_probe_results.push_back(TlsProberReturnValue{
            net::ERR_TIMED_OUT,
            TlsProber::ProbeExitEnum::kMojoDisconnectFailure});
      }
    }
  }
  SetUpAndRunRoutine(std::move(fake_probe_results),
                     mojom::RoutineVerdict::kNoProblem, {});
}

}  // namespace ash::network_diagnostics
