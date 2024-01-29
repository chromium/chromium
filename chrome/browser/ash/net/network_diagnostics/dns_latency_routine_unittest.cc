// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/dns_latency_routine.h"

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/net/network_diagnostics/fake_network_context.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

const int kFakePortNumber = 1234;
const char kFakeTestProfile[] = "test";
const base::TimeDelta kSuccessfulDnsResolutionDelayMs = base::Milliseconds(100);
const base::TimeDelta kSlightlyAboveThresholdDelayMs = base::Milliseconds(450);
const base::TimeDelta kSignificantlyAboveThresholdDelayMs =
    base::Milliseconds(550);

class FakeTickClock : public base::TickClock {
 public:
  // The |dns_resolution_delay| fakes the duration of a DNS resolution.
  explicit FakeTickClock(
      const base::TimeDelta& dns_resolution_delay = base::TimeDelta())
      : dns_resolution_delay_(dns_resolution_delay) {}

  ~FakeTickClock() override = default;

  base::TimeTicks NowTicks() const override {
    base::TimeTicks current = current_time_;
    // Advance the current time by |dns_resolution_delay_| so that each
    // NowTicks() invocation will have this delay. This allows tests to mimic
    // realistic time conditions.
    current_time_ = current_time_ + dns_resolution_delay_;
    return current;
  }

 private:
  mutable base::TimeTicks current_time_ = base::TimeTicks::Now();
  const base::TimeDelta dns_resolution_delay_;
};

net::IPEndPoint FakeIPAddress() {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), kFakePortNumber);
}

}  // namespace

class DnsLatencyRoutineTest : public ::testing::Test {
 public:
  DnsLatencyRoutineTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    session_manager::SessionManager::Get()->SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);
  }

  DnsLatencyRoutineTest(const DnsLatencyRoutineTest&) = delete;
  DnsLatencyRoutineTest& operator=(const DnsLatencyRoutineTest&) = delete;

  void SetUpFakeProperties(
      std::unique_ptr<FakeNetworkContext::DnsResult> fake_dns_result,
      const base::TimeDelta response_latency) {
    ASSERT_TRUE(profile_manager()->SetUp());

    fake_network_context_ = std::make_unique<FakeNetworkContext>();
    fake_network_context_->set_fake_dns_result(std::move(fake_dns_result));
    test_profile_ = profile_manager()->CreateTestingProfile(kFakeTestProfile);
    fake_tick_clock_ = std::make_unique<FakeTickClock>(response_latency);
  }

  void SetUpDnsLatencyRoutine() {
    dns_latency_routine_ = std::make_unique<DnsLatencyRoutine>(
        mojom::RoutineCallSource::kDiagnosticsUI);
    dns_latency_routine_->set_network_context_for_testing(
        fake_network_context_.get());
    dns_latency_routine_->set_profile_for_testing(test_profile_);
    dns_latency_routine_->set_tick_clock_for_testing(fake_tick_clock_.get());
  }

  void RunRoutine(
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::DnsLatencyProblem>& expected_problems) {
    base::test::TestFuture<mojom::RoutineResultPtr> future;
    dns_latency_routine_->RunRoutine(future.GetCallback());
    auto result = future.Take();
    EXPECT_EQ(expected_routine_verdict, result->verdict);
    EXPECT_EQ(expected_problems, result->problems->get_dns_latency_problems());
  }

  // Sets up required properties (via fakes) and runs the test.
  //
  // Parameters:
  // |fake_dns_result|: Represents the result of a DNS resolution.
  // |response_latency|: Represents the time elapsed while performing the DNS
  // resolution.
  // |routine_verdict|: Represents the expected verdict reported by this test.
  // |expected_problems|: Represents the expected problem reported by this test.
  void SetUpAndRunRoutine(
      std::unique_ptr<FakeNetworkContext::DnsResult> fake_dns_result,
      const base::TimeDelta response_latency,
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::DnsLatencyProblem>& expected_problems) {
    SetUpFakeProperties(std::move(fake_dns_result), response_latency);
    SetUpDnsLatencyRoutine();
    RunRoutine(expected_routine_verdict, expected_problems);
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

 protected:
  base::WeakPtr<DnsLatencyRoutineTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeNetworkContext> fake_network_context_;
  raw_ptr<Profile, DanglingUntriaged> test_profile_;
  std::unique_ptr<FakeTickClock> fake_tick_clock_;
  session_manager::SessionManager session_manager_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<DnsLatencyRoutine> dns_latency_routine_;
  base::WeakPtrFactory<DnsLatencyRoutineTest> weak_factory_{this};
};

// A passing routine requires an error code of net::OK and a non-empty
// net::AddressList for each DNS resolution and an averagelatency below the
// allowed threshold.
TEST_F(DnsLatencyRoutineTest, TestSuccessfulResolutions) {
  auto fake_dns_result = std::make_unique<FakeNetworkContext::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK),
      net::AddressList(FakeIPAddress()),
      /*endpoint_results_with_metadata=*/std::nullopt);
  SetUpAndRunRoutine(std::move(fake_dns_result),
                     kSuccessfulDnsResolutionDelayMs,
                     mojom::RoutineVerdict::kNoProblem, {});
}

// Set up the |fake_dns_result| to return a DnsResult with an error code
// net::ERR_NAME_NOT_RESOLVED, faking a failed resolution attempt.
TEST_F(DnsLatencyRoutineTest, TestUnsuccessfulResolution) {
  auto fake_dns_result = std::make_unique<FakeNetworkContext::DnsResult>(
      net::ERR_NAME_NOT_RESOLVED,
      net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
      /*resolved_addresses=*/std::nullopt,
      /*endpoint_results_with_metadata=*/std::nullopt);
  // The time taken to complete the resolution is not important for this test,
  // because a failed resolution attempt already results in a problem.
  SetUpAndRunRoutine(std::move(fake_dns_result),
                     kSuccessfulDnsResolutionDelayMs,
                     mojom::RoutineVerdict::kProblem,
                     {mojom::DnsLatencyProblem::kHostResolutionFailure});
}

// This test case represents the scenario where a DNS resolution was successful;
// however, the average resolution latency was slightly above the allowed
// threshold.
TEST_F(DnsLatencyRoutineTest, TestLatencySlightlyAboveThreshold) {
  auto fake_dns_result = std::make_unique<FakeNetworkContext::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK),
      net::AddressList(FakeIPAddress()),
      /*endpoint_results_with_metadata=*/std::nullopt);
  SetUpAndRunRoutine(std::move(fake_dns_result), kSlightlyAboveThresholdDelayMs,
                     mojom::RoutineVerdict::kProblem,
                     {mojom::DnsLatencyProblem::kSlightlyAboveThreshold});
}

// This test case represents the scenario where the DNS resolutions were
// successful; however, the average resolution latency was significantly above
// the allowed threshold.
TEST_F(DnsLatencyRoutineTest, TestLatencySignificantlyAboveThreshold) {
  auto fake_dns_result = std::make_unique<FakeNetworkContext::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK),
      net::AddressList(FakeIPAddress()),
      /*endpoint_results_with_metadata=*/std::nullopt);
  SetUpAndRunRoutine(std::move(fake_dns_result),
                     kSignificantlyAboveThresholdDelayMs,
                     mojom::RoutineVerdict::kProblem,
                     {mojom::DnsLatencyProblem::kSignificantlyAboveThreshold});
}

}  // namespace ash::network_diagnostics
