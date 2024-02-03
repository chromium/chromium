// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/dns_resolution_routine.h"

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
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

net::IPEndPoint FakeIPAddress() {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), kFakePortNumber);
}

}  // namespace

class DnsResolutionRoutineTest : public ::testing::Test {
 public:
  DnsResolutionRoutineTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    session_manager::SessionManager::Get()->SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);
  }
  DnsResolutionRoutineTest(const DnsResolutionRoutineTest&) = delete;
  DnsResolutionRoutineTest& operator=(const DnsResolutionRoutineTest&) = delete;

  void RunRoutine(
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::DnsResolutionProblem>& expected_problems) {
    base::test::TestFuture<mojom::RoutineResultPtr> future;
    dns_resolution_routine_->RunRoutine(future.GetCallback());
    auto result = future.Take();
    EXPECT_EQ(expected_routine_verdict, result->verdict);
    EXPECT_EQ(expected_problems,
              result->problems->get_dns_resolution_problems());
  }

  void SetUpFakeProperties(
      base::circular_deque<std::unique_ptr<FakeNetworkContext::DnsResult>>
          fake_dns_results) {
    ASSERT_TRUE(profile_manager_.SetUp());

    fake_network_context_ = std::make_unique<FakeNetworkContext>();
    fake_network_context_->set_fake_dns_results(std::move(fake_dns_results));
    test_profile_ = profile_manager_.CreateTestingProfile(kFakeTestProfile);
  }

  void SetUpDnsResolutionRoutine() {
    dns_resolution_routine_ = std::make_unique<DnsResolutionRoutine>(
        mojom::RoutineCallSource::kDiagnosticsUI);
    dns_resolution_routine_->set_network_context_for_testing(
        fake_network_context_.get());
    dns_resolution_routine_->set_profile_for_testing(test_profile_);
  }

  // Sets up required properties (via fakes) and runs the test.
  //
  // Parameters:
  // |fake_dns_results|: Represents the results of a one or more DNS
  // resolutions. |expected_routine_verdict|: Represents the expected verdict
  // reported by this test. |expected_problems|: Represents the expected problem
  // reported by this test.
  void SetUpAndRunRoutine(
      base::circular_deque<std::unique_ptr<FakeNetworkContext::DnsResult>>
          fake_dns_results,
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::DnsResolutionProblem>& expected_problems) {
    SetUpFakeProperties(std::move(fake_dns_results));
    SetUpDnsResolutionRoutine();
    RunRoutine(expected_routine_verdict, expected_problems);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<FakeNetworkContext> fake_network_context_;
  // Unowned
  raw_ptr<Profile, DanglingUntriaged> test_profile_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<DnsResolutionRoutine> dns_resolution_routine_;
  base::WeakPtrFactory<DnsResolutionRoutineTest> weak_factory_{this};
};

// A passing routine requires an error code of net::OK and a non-empty
// net::AddressList for the DNS resolution.
TEST_F(DnsResolutionRoutineTest, TestSuccessfulResolution) {
  base::circular_deque<std::unique_ptr<FakeNetworkContext::DnsResult>>
      fake_dns_results;
  auto successful_resolution = std::make_unique<FakeNetworkContext::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK),
      net::AddressList(FakeIPAddress()),
      /*endpoint_results_with_metadata=*/std::nullopt);
  fake_dns_results.push_back(std::move(successful_resolution));
  SetUpAndRunRoutine(std::move(fake_dns_results),
                     mojom::RoutineVerdict::kNoProblem, {});
}

// Set up the |fake_dns_results| to return a DnsResult with an error code
// net::ERR_NAME_NOT_RESOLVED faking a failed DNS resolution.
TEST_F(DnsResolutionRoutineTest, TestResolutionFailure) {
  base::circular_deque<std::unique_ptr<FakeNetworkContext::DnsResult>>
      fake_dns_results;
  auto failed_resolution = std::make_unique<FakeNetworkContext::DnsResult>(
      net::ERR_NAME_NOT_RESOLVED,
      net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
      /*resolved_addresses=*/std::nullopt,
      /*endpoint_results_with_metadata=*/std::nullopt);
  fake_dns_results.push_back(std::move(failed_resolution));
  SetUpAndRunRoutine(std::move(fake_dns_results),
                     mojom::RoutineVerdict::kProblem,
                     {mojom::DnsResolutionProblem::kFailedToResolveHost});
}

// Set up the |fake_dns_results| to first return a DnsResult with an error code
// net::ERR_DNS_TIMED_OUT faking a timed out DNS resolution. On the second
// host resolution attempt, fake a net::OK resolution.
TEST_F(DnsResolutionRoutineTest, TestSuccessOnRetry) {
  base::circular_deque<std::unique_ptr<FakeNetworkContext::DnsResult>>
      fake_dns_results;
  auto timed_out_resolution = std::make_unique<FakeNetworkContext::DnsResult>(
      net::ERR_DNS_TIMED_OUT, net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
      /*resolved_addresses=*/std::nullopt,
      /*endpoint_results_with_metadata=*/std::nullopt);
  fake_dns_results.push_back(std::move(timed_out_resolution));
  auto successful_resolution = std::make_unique<FakeNetworkContext::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK),
      net::AddressList(FakeIPAddress()),
      /*endpoint_results_with_metadata=*/std::nullopt);
  fake_dns_results.push_back(std::move(successful_resolution));

  fake_dns_results.push_back(std::move(successful_resolution));
  SetUpAndRunRoutine(std::move(fake_dns_results),
                     mojom::RoutineVerdict::kNoProblem, {});
}

}  // namespace ash::network_diagnostics
